/* Spa A2DP LDAC codec
 *
 * Copyright © 2020 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <arpa/inet.h>

#include <spa/param/audio/format.h>

#include <ldacBT.h>

#include "defs.h"
#include "rtp.h"
#include "a2dp-codecs.h"

#define MAX_FRAME_COUNT 16

struct impl {
	HANDLE_LDAC_BT ldac;

	struct rtp_header *header;
	struct rtp_payload *payload;

	int mtu;
	int eqmid;
	int channel_mode;
	int frequency;
	int fmt;
	int lsu;
	int codesize;
	int frame_length;
};

static int codec_fill_caps(uint32_t flags, uint8_t caps[A2DP_MAX_CAPS_SIZE])
{
	const a2dp_ldac_t a2dp_ldac = {
		.info.vendor_id = LDAC_VENDOR_ID,
		.info.codec_id = LDAC_CODEC_ID,
		.frequency = LDACBT_SAMPLING_FREQ_044100 |
			LDACBT_SAMPLING_FREQ_048000 |
			LDACBT_SAMPLING_FREQ_088200 |
			LDACBT_SAMPLING_FREQ_096000 |
			LDACBT_SAMPLING_FREQ_176400 |
			LDACBT_SAMPLING_FREQ_192000,
		.channel_mode = LDACBT_CHANNEL_MODE_MONO |
			LDACBT_CHANNEL_MODE_DUAL_CHANNEL |
			LDACBT_CHANNEL_MODE_STEREO,
	};
	memcpy(caps, &a2dp_ldac, sizeof(a2dp_ldac));
	return sizeof(a2dp_ldac);
}

static int codec_select_config(uint32_t flags, const void *caps, size_t caps_size,
			const struct spa_audio_info *info, uint8_t config[A2DP_MAX_CAPS_SIZE])
{
	a2dp_ldac_t conf;

        if (caps_size < sizeof(conf))
                return -EINVAL;

	memcpy(&conf, caps, sizeof(conf));
	conf.info.vendor_id = LDAC_VENDOR_ID;
	conf.info.codec_id = LDAC_CODEC_ID;

	if (conf.frequency & LDACBT_SAMPLING_FREQ_044100)
		conf.frequency = LDACBT_SAMPLING_FREQ_044100;
	if (conf.frequency & LDACBT_SAMPLING_FREQ_048000)
		conf.frequency = LDACBT_SAMPLING_FREQ_048000;
	if (conf.frequency & LDACBT_SAMPLING_FREQ_088200)
		conf.frequency = LDACBT_SAMPLING_FREQ_088200;
	if (conf.frequency & LDACBT_SAMPLING_FREQ_096000)
		conf.frequency = LDACBT_SAMPLING_FREQ_096000;
	if (conf.frequency & LDACBT_SAMPLING_FREQ_176400)
		conf.frequency = LDACBT_SAMPLING_FREQ_176400;
	if (conf.frequency & LDACBT_SAMPLING_FREQ_192000)
		conf.frequency = LDACBT_SAMPLING_FREQ_192000;
	else
		return -ENOTSUP;

	if (conf.channel_mode & LDACBT_CHANNEL_MODE_STEREO)
		conf.channel_mode = LDACBT_CHANNEL_MODE_STEREO;
        else if (conf.channel_mode & LDACBT_CHANNEL_MODE_DUAL_CHANNEL)
		conf.channel_mode = LDACBT_CHANNEL_MODE_DUAL_CHANNEL;
        else if (conf.channel_mode & LDACBT_CHANNEL_MODE_MONO)
		conf.channel_mode = LDACBT_CHANNEL_MODE_MONO;
	else
		return -ENOTSUP;

	memcpy(config, &conf, sizeof(conf));

        return sizeof(conf);
}

static int codec_reduce_bitpool(void *data)
{
	struct impl *this = data;
	return ldacBT_alter_eqmid_priority(this->ldac, LDACBT_EQMID_INC_CONNECTION);
}

static int codec_increase_bitpool(void *data)
{
	struct impl *this = data;
	return ldacBT_alter_eqmid_priority(this->ldac, LDACBT_EQMID_INC_QUALITY);
}

static int codec_get_num_blocks(void *data, size_t mtu)
{
	struct impl *this = data;
	size_t rtp_size = sizeof(struct rtp_header) + sizeof(struct rtp_payload);
	size_t frame_count = (mtu - rtp_size) / this->frame_length;

	this->mtu = mtu;

	/* frame_count is only 4 bit number */
	if (frame_count > 15)
		frame_count = 15;
	return frame_count;
}

static int codec_get_block_size(void *data)
{
	struct impl *this = data;
	return this->codesize;
}

static void *codec_init(uint32_t flags, void *config, size_t config_len, struct spa_audio_info *info)
{
	struct impl *this;
	a2dp_ldac_t *conf = config;
	int res;

	this = calloc(1, sizeof(struct impl));
	if (this == NULL)
		goto error_errno;

	this->ldac = ldacBT_get_handle();
	if (this->ldac == NULL)
		goto error_errno;

	this->eqmid = LDACBT_EQMID_SQ;

	spa_zero(*info);
	info->media_type = SPA_MEDIA_TYPE_audio;
	info->media_subtype = SPA_MEDIA_SUBTYPE_raw;
	info->info.raw.format = SPA_AUDIO_FORMAT_S16;
	this->fmt = LDACBT_SMPL_FMT_S16;

	switch(conf->frequency) {
	case LDACBT_SAMPLING_FREQ_044100:
		info->info.raw.rate = 44100;
		this->lsu = 128;
		break;
	case LDACBT_SAMPLING_FREQ_048000:
		info->info.raw.rate = 48000;
		this->lsu = 128;
		break;
	case LDACBT_SAMPLING_FREQ_088200:
		info->info.raw.rate = 88200;
		this->lsu = 256;
		break;
	case LDACBT_SAMPLING_FREQ_096000:
		info->info.raw.rate = 96000;
		this->lsu = 256;
		break;
	case LDACBT_SAMPLING_FREQ_176400:
		info->info.raw.rate = 176400;
		this->lsu = 512;
		break;
	case LDACBT_SAMPLING_FREQ_192000:
		info->info.raw.rate = 192000;
		this->lsu = 512;
		break;
	default:
		res = -EINVAL;
		goto error;
	}
	this->frequency = info->info.raw.rate;

	switch(conf->channel_mode) {
	case LDACBT_CHANNEL_MODE_STEREO:
		info->info.raw.channels = 2;
		break;
	case LDACBT_CHANNEL_MODE_DUAL_CHANNEL:
		info->info.raw.channels = 2;
		break;
	case LDACBT_CHANNEL_MODE_MONO:
		info->info.raw.channels = 1;
		break;
	default:
		res = -EINVAL;
		goto error;
	}
	this->channel_mode = conf->channel_mode;

	switch (this->eqmid) {
	case LDACBT_EQMID_HQ:
		this->frame_length = 330;
		break;
	case LDACBT_EQMID_SQ:
		this->frame_length = 220;
		break;
	case LDACBT_EQMID_MQ:
		this->frame_length = 110;
		break;
	default:
		res = -EINVAL;
		goto error;
	}

	res = ldacBT_init_handle_encode(this->ldac,
			this->mtu,
			this->eqmid,
			this->channel_mode,
			this->fmt,
			this->frequency);
	if (res < 0)
		goto error;

	this->codesize = this->lsu * info->info.raw.channels * 2;

	return this;

error_errno:
	res = -errno;
error:
	if (this->ldac)
		ldacBT_free_handle(this->ldac);
	free(this);
	errno = -res;
	return NULL;
}

static void codec_deinit(void *data)
{
	struct impl *this = data;
	if (this->ldac)
		ldacBT_free_handle(this->ldac);
	free(this);
}

static int codec_start_encode (void *data,
		void *dst, size_t dst_size, uint16_t seqnum, uint32_t timestamp)
{
	struct impl *this = data;

	this->header = (struct rtp_header *)dst;
	this->payload = SPA_MEMBER(dst, sizeof(struct rtp_header), struct rtp_payload);
	memset(this->header, 0, sizeof(struct rtp_header)+sizeof(struct rtp_payload));

	this->payload->frame_count = 0;
	this->header->v = 2;
	this->header->pt = 1;
	this->header->sequence_number = htons(seqnum);
	this->header->timestamp = htonl(timestamp);
	this->header->ssrc = htonl(1);
	return sizeof(struct rtp_header) + sizeof(struct rtp_payload);
}

static int codec_encode(void *data,
		const void *src, size_t src_size,
		void *dst, size_t dst_size,
		size_t *dst_out)
{
	struct impl *this = data;
	int res, src_used, dst_used, frame_num = 0;

	src_used = src_size;
	dst_used = dst_size;

	res = ldacBT_encode(this->ldac, (void*)src, &src_used, dst, &dst_used, &frame_num);
	if (res < 0)
		return -EINVAL;

	*dst_out = dst_used;

	this->payload->frame_count += frame_num;

	return src_used;
}

struct a2dp_codec a2dp_codec_ldac = {
	.id = {.codec_id = A2DP_CODEC_VENDOR,
		.vendor_id = LDAC_VENDOR_ID,
		.vendor_codec_id = LDAC_CODEC_ID },
	.name = "ldac",
	.description = "LDAC",
	.fill_caps = codec_fill_caps,
	.select_config = codec_select_config,
	.init = codec_init,
	.deinit = codec_deinit,
	.get_block_size = codec_get_block_size,
	.get_num_blocks = codec_get_num_blocks,
	.start_encode = codec_start_encode,
	.encode = codec_encode,
	.reduce_bitpool = codec_reduce_bitpool,
	.increase_bitpool = codec_increase_bitpool,
};
