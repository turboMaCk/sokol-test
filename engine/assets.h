#ifndef ASSETS_H
#define ASSETS_H

#include "sokol_gfx.h"
#include "la_math.h"
#include "stb_image.h"

// Texture
// layout: [ kind:3 | size:5 ]
#define PIXEL_SIZE_BITS 5
#define PIXEL_SIZE_MASK ((1u<<PIXEL_SIZE_BITS)-1)

typedef enum {
    PixelFormatUnknown = 0,
    PixelFormatRgba = (1u << PIXEL_SIZE_BITS) | 4u, // kind 1, size 4
    PixelFormatR8   = (2u << PIXEL_SIZE_BITS) | 1u, // kind 3, size 1
} PixelFormat;

typedef struct {
    sg_image image;
    sg_view view;
    PixelFormat pixel_format;
    Vec2i size;
} Texture;

Texture texture_create_rgba(Vec2i size, void* data);
Texture texture_create_r(Vec2i size, void* data);
void texture_destroy(Texture* texture);
void texture_update(Texture* texture, void* data);
Texture texture_load_png(const char* path);
bool texture_valid(Texture* texture);

typedef struct {
    Texture texture;
} TextureAtlas;

TextureAtlas texture_atlas_from_texture(Texture texture);

#endif // ASSETS_H

#if defined(ENGINE_IMPL) || defined(__CLANGD__)

#ifndef ENGINE_ASSETS_IMPL_GUARD
#define ENGINE_ASSETS_IMPL_GUARD

#include <string.h>

static inline uint8_t pixel_size(PixelFormat pf) {
    return (uint8_t)pf & PIXEL_SIZE_MASK;
}

static inline sg_pixel_format pixel_format_to_sg(PixelFormat pf) {
    switch(pf) {
    case PixelFormatUnknown: return SG_PIXELFORMAT_NONE;
    case PixelFormatRgba:    return SG_PIXELFORMAT_RGBA8;
    case PixelFormatR8:      return SG_PIXELFORMAT_R8;
    default:                 return SG_PIXELFORMAT_NONE;
    }
}

static inline Texture texture_create(Vec2i size, void* data, PixelFormat format) {
    sg_image_desc img_desc = {
        .width = size.x,
        .height = size.y,
        .pixel_format = pixel_format_to_sg(format),
        .usage = {
            // TODO: for simplicity all textures will be updatable
            // but it comes with some small runtime cost
            // and might not be needed at all?
            .dynamic_update = true
        },
    };
    sg_image image = sg_make_image(&img_desc);

    sg_view_desc view_desc = {
        .texture = {
            .image = image
        }
    };

    sg_view view = sg_make_view(&view_desc);

    Texture texture = (Texture) {
        .image = image,
        .view = view,
        .pixel_format = format,
        .size = size,
    };

    texture_update(&texture, data);

    return texture;
}

Texture texture_create_rgba(Vec2i size, void* data) {
    return texture_create(size, data, PixelFormatRgba);
}

Texture texture_create_r(Vec2i size, void* data) {
    return texture_create(size, data, PixelFormatR8);
}

void texture_destroy(Texture* texture) {
    sg_destroy_view(texture->view);
    sg_destroy_image(texture->image);
    memset(texture, 0, sizeof(*texture));
}

void texture_update(Texture* texture, void* data) {
    sg_image_data img_data = {
        .mip_levels[0] = {
            .ptr = data,
            .size = texture->size.x * texture->size.y * pixel_size(texture->pixel_format)
        }
    };

    sg_update_image(texture->image, &img_data);
}


Texture texture_load_png(const char* path) {
    int width, height, channels;

    unsigned char* pixels = stbi_load(path, &width, &height, &channels, 4 /* RGBA */);

    if (!pixels) {
        // TODO: should we crash hard?
        return (Texture){0};
    }

    Texture texture = texture_create_rgba((Vec2i){width, height}, pixels);
    stbi_image_free(pixels);

    return texture;
}

bool texture_valid(Texture* texture) {
    return texture && sg_query_image_state(texture->image) == SG_RESOURCESTATE_VALID;
}

TextureAtlas texture_atlas_from_texture(Texture texture) {
    return (TextureAtlas) { texture };
}

#endif // ENGINE_ASSETS_IMPL_GUARD
#endif // ENGINE_IMPL
