/* PipeWire
 *
 * Copyright © 2021 Wim Taymans
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

#ifndef PIPEWIRE_IMPL_METADATA_H
#define PIPEWIRE_IMPL_METADATA_H

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup pw_impl_metadata Pipewire Metadata Interface
 *
 * The metadata is used to store key/type/value pairs per object id.
 */

/**
 * \addtogroup pw_impl_metadata
 * \{
 */
struct pw_impl_metadata;

#include <pipewire/context.h>
#include <pipewire/impl-client.h>
#include <pipewire/global.h>
#include <pipewire/properties.h>
#include <pipewire/resource.h>

#include <extensions/metadata.h>

/** Metadata events, listen to them with \ref pw_impl_metadata_add_listener */
struct pw_impl_metadata_events {
#define PW_VERSION_IMPL_METADATA_EVENTS	0
	uint32_t version;

	/** the metadata is destroyed */
        void (*destroy) (void *data);
	/** the metadata is freed */
        void (*free) (void *data);

	/** a property changed */
	int (*property) (void *data,
			uint32_t subject,
			const char *key,
			const char *type,
			const char *value);
};

struct pw_impl_metadata *pw_context_create_metadata(struct pw_context *context,
		const char *name, struct pw_properties *properties,
		size_t user_data_size);

/** Get the metadata properties */
const struct pw_properties *pw_impl_metadata_get_properties(struct pw_impl_metadata *metadata);

int pw_impl_metadata_register(struct pw_impl_metadata *metadata,
			struct pw_properties *properties);

void pw_impl_metadata_destroy(struct pw_impl_metadata *metadata);

void *pw_impl_metadata_get_user_data(struct pw_impl_metadata *metadata);

int pw_impl_metadata_set_implementation(struct pw_impl_metadata *metadata,
		struct pw_metadata *impl);

struct pw_metadata *pw_impl_metadata_get_implementation(struct pw_impl_metadata *metadata);

/** Get the global of this metadata */
struct pw_global *pw_impl_metadata_get_global(struct pw_impl_metadata *metadata);

/** Add an event listener */
void pw_impl_metadata_add_listener(struct pw_impl_metadata *metadata,
			     struct spa_hook *listener,
			     const struct pw_impl_metadata_events *events,
			     void *data);

/** Set a property */
int pw_impl_metadata_set_property(struct pw_impl_metadata *metadata,
			uint32_t subject, const char *key, const char *type,
			const char *value);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_IMPL_METADATA_H */
