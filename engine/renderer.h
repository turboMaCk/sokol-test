#ifndef RENDERER_H
#define RENDERER_H

#include "sokol_gfx.h"

#define MAX_SPRITES 2048
#define MAX_VERTICES (MAX_SPRITES * 4) // 4 vertices per quad
#define MAX_INDICES (MAX_SPRITES * 6) // 6 indices per quad

// TODO: probably move to math module
typedef struct { float x, y; } Vec2;
typedef struct { int x, y; } Vec2i;
typedef struct { float m[16]; } Mat4;
Vec2 vec2_normalize(Vec2 v);

Vec2 rotation_from_rad(float radians);
Vec2 rotation_from_deg(float degrees);
Vec2 rotation_mul(Vec2 a, Vec2 b);
Vec2 rotation_inverse(Vec2 r);
Mat4 mat4_ortho(float left, float right, float bottom, float top);

#define ROTATION_NONE ((Vec2){ 1.0f, 0.0f })

#define ROTATION_45        ((Vec2){  0.70710678f,  0.70710678f })
#define ROTATION_NEG_45    ((Vec2){  0.70710678f, -0.70710678f })

#define ROTATION_90        ((Vec2){  0.0f,  1.0f })
#define ROTATION_NEG_90    ((Vec2){  0.0f, -1.0f })

#define ROTATION_180       ((Vec2){ -1.0f,  0.0f })
#define ROTATION_NEG_180   ((Vec2){ -1.0f,  0.0f })

// layout: [ kind:3 | size:5 ]
#define PIXEL_SIZE_BITS 5
#define PIXEL_SIZE_MASK ((1u<<PIXEL_SIZE_BITS)-1)

typedef enum uint8_t {
    PixelFormatUnknown = 0,
    PixelFormatRgba = (1u << PIXEL_SIZE_BITS) | 4u, // kind 1, size 4
    PixelFormatRgb  = (2u << PIXEL_SIZE_BITS) | 3u, // kind 2, size 3
    PixelFormatR    = (3u << PIXEL_SIZE_BITS) | 1u, // kind 3, size 1
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
Texture texture_white_pixel(void);

typedef struct {
    sg_sampler sampler;
} Sampler;

Sampler sampler_create_default(void);
void sampler_destroy(Sampler* sampler);

typedef struct {
    sg_image color_img;
    sg_image depth_img;
    sg_view color_view;
    sg_view depth_view;
    sg_view color_texture_view;
    Vec2i size;
    bool is_swapchain;
} RenderTarget;

RenderTarget render_target_create_offscreen(int width, int height);
RenderTarget render_target_create_swapchain(void);
void render_target_destroy(RenderTarget *target);
void renderer_begin_target_pass(RenderTarget* target, sg_color clear_color);
void renderer_end_target_pass(RenderTarget* target);
static inline float aspect_of(Vec2i rect);

typedef struct {
    float position[3];
    float color[4];
    Vec2 uv;
} Vertex;

typedef struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pipeline pip;

    Vertex vertex_buffer[MAX_VERTICES];
    int vertex_count;

    // current projection, applied during renderer_begin()
    Mat4 projection;

    sg_image white_image;

    // The persistent sampler initialized once during setup
    sg_sampler default_sampler;

    // TODO: these live in here temporarily
    // they will be moved to sprite manager
    sg_view white_tex_view;
    sg_view current_texture_view;
} RenderBatch2d;

typedef struct {
    Vec2 position;
    Vec2 size;
    Vec2 rotation; // Stores { cos(angle), sin(angle) }
    sg_color color;
    sg_view texture_view;
} Sprite;

void renderer_init(RenderBatch2d* renderer, sg_shader shader);
void renderer_begin(RenderBatch2d* renderer, Mat4 projection);
void renderer_flush(RenderBatch2d* renderer);
void renderer_end(RenderBatch2d* renderer);
void renderer_push_sprite(RenderBatch2d* renderer, Sprite* sprite);
void renderer_destroy(RenderBatch2d* renderer);
void sprite_fit_to(Sprite *sprite, RenderTarget *source, RenderTarget *target);

#endif // RENDERER_H

#if defined(ENGINE_IMPL) || defined(__CLANGD__)

#ifndef ENGINE_IMPL_GUARD
#define ENGINE_IMPL_GUARD

#include <string.h>
#include <math.h>
#include "sokol_app.h"

// Math


Vec2 vec2_normalize(Vec2 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len == 0.0f) return (Vec2){0};
    return (Vec2) {
        .x = v.x / len,
        .y = v.y / len,
    };
}

Vec2 rotation_from_rad(float radians) {
    Vec2 rot;
    rot.x = cosf(radians);
    rot.y = sinf(radians);
    return rot;
}

Vec2 rotation_from_deg(float degrees) {
    // Convert degrees to radians: (deg * PI / 180)
    // Using 0.0174532925f (PI / 180) avoids a runtime division
    float radians = degrees * 0.0174532925f;
    return rotation_from_rad(radians);
}

// Multiplication of 2 complex numbers is like angle addition
Vec2 rotation_mul(Vec2 a, Vec2 b) {
    return (Vec2){
        .x = a.x * b.x - a.y * b.y,
        .y = a.y * b.x + a.x * b.y
    };
}

Vec2 rotation_inverse(Vec2 r) {
    return (Vec2) {r.x, -r.y};
}

Mat4 mat4_ortho(float left, float right, float bottom, float top) {
    Mat4 result = {0};

    // Near/Far are mapped tightly to [-1.0, 1.0] for 2D depth layers
    float near_val = -1.0f;
    float far_val  =  1.0f;

    result.m[0]  = 2.0f / (right - left);
    result.m[5]  = 2.0f / (top - bottom);
    result.m[10] = -2.0f / (far_val - near_val);
    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(top + bottom) / (top - bottom);
    result.m[14] = -(far_val + near_val) / (far_val - near_val);
    result.m[15] = 1.0f;

    return result;
}

// Texture

static inline uint8_t pixel_size(PixelFormat pf) {
    return (uint8_t)pf & PIXEL_SIZE_MASK;
}

static inline sg_pixel_format pixel_format_to_sg(PixelFormat pf) {
    switch(pf) {
    case PixelFormatUnknown: return SG_PIXELFORMAT_NONE;
    case PixelFormatRgba:    return SG_PIXELFORMAT_RGBA8;
    case PixelFormatRgb:     return SG_PIXELFORMAT_SRGB8A8;
    case PixelFormatR:       return SG_PIXELFORMAT_R8;
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
        .data.mip_levels[0] = {
            .ptr = data,
            .size = size.x * size.y * pixel_size(format),
        }
    };
    sg_image image = sg_make_image(&img_desc);

    sg_view_desc view_desc = {
        .texture = {
            .image = image
        }
    };

    sg_view view = sg_make_view(&view_desc);

    return (Texture) {
        .image = image,
        .view = view,
        .pixel_format = format,
        .size = size,
    };
}

Texture texture_create_rgba(Vec2i size, void* data) {
    return texture_create(size, data, PixelFormatRgba);
}

Texture texture_create_r(Vec2i size, void* data) {
    return texture_create(size, data, PixelFormatR);
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

Texture texture_white_pixel(void) {
    static uint32_t white_pixel = 0xFFFFFFFF;
    return texture_create_rgba((Vec2i){1,1}, &white_pixel);
}

// Sampler

Sampler sampler_create_default(void) {
    sg_sampler sampler = sg_make_sampler(&(sg_sampler_desc){ .label = "default-sampler" });

    return (Sampler){sampler};
}

void sampler_destroy(Sampler* sampler) {
    sg_destroy_sampler(sampler->sampler);
    memset(sampler, 0, sizeof(Sampler));
}

// RenderTarget

// Implementation functions
RenderTarget render_target_create_offscreen(int width, int height) {
    RenderTarget target = {
        .size = (Vec2i) {width, height},
        .is_swapchain = false
    };

    target.color_img = sg_make_image(&(sg_image_desc){
        .width = width,
        .height = height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .usage = {
            .color_attachment = true
        },
        .label = "offscreen-color-target"
    });

    target.depth_img = sg_make_image(&(sg_image_desc){
        .width = (int)width,
        .height = (int)height,
        .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
        .sample_count = 1,
        .usage = {
            .depth_stencil_attachment = true
        },
        .label = "offscreen-depth-target"
    });

    target.color_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = target.color_img,
        .label = "offscreen-color-view"
    });

    target.depth_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = target.depth_img,
        .label = "offscreen-depth-view"
    });

    target.color_texture_view = sg_make_view(&(sg_view_desc){
        .texture.image = target.color_img,
        .label = "offscreen-color-texture-view"
    });

    return target;
}

RenderTarget render_target_create_swapchain(void) {
    return (RenderTarget){
        .size = (Vec2i){0},   // Will be overwritten with actual window width on frame 1
        .is_swapchain = true
    };
}

void renderer_begin_target_pass(RenderTarget* target, sg_color clear_color) {
    sg_pass_action action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = clear_color
        }
    };

    sg_pass pass = { .action = action };

    if (target->is_swapchain) {
        extern sg_swapchain sglue_swapchain(void);

        // Keep our internal engine tracking dimensions updated in case of resize
        target->size.x = sapp_width();
        target->size.y = sapp_height();

        // Let the glue library completely manage the swapchain population
        pass.swapchain = sglue_swapchain();
    } else {
        // Direct view assignments for offscreen rendering
        pass.attachments.colors[0] = target->color_view;
        pass.attachments.depth_stencil = target->depth_view;
    }

    sg_begin_pass(&pass);
}

void renderer_end_target_pass(RenderTarget* target) {
    sg_end_pass();
}

void render_target_destroy(RenderTarget *target) {
    if (target->is_swapchain) return;
    sg_destroy_image(target->color_img);
    sg_destroy_image(target->depth_img);
    sg_destroy_view(target->color_view);
    sg_destroy_view(target->depth_view);
    sg_destroy_view(target->color_texture_view);
    target->size = (Vec2i){0};
    target->is_swapchain = true;
}

static inline float aspect_of(Vec2i rect) {
    return (float)rect.x / (float)rect.y;
}

// Renderer

void renderer_init(RenderBatch2d* renderer, sg_shader shader) {
    memset(renderer, 0, sizeof(RenderBatch2d));

    // vertex buffer
    sg_buffer_desc vbuf_desc = {
        .size = sizeof(Vertex) * MAX_VERTICES,
        .usage = {
            .dynamic_update = true,
            .vertex_buffer = true
        }
    };

    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);
    renderer->vbuf = vbuf;

    // index buffer
    uint16_t indices[MAX_SPRITES * 6];
    int curr_vertex = 0;
    for (int i = 0; i < (MAX_SPRITES * 6); i += 6) {
        // first triangle
        indices[i + 0] = curr_vertex + 0;
        indices[i + 1] = curr_vertex + 1;
        indices[i + 2] = curr_vertex + 2;
        // second triangle
        indices[i + 3] = curr_vertex + 2;
        indices[i + 4] = curr_vertex + 3;
        indices[i + 5] = curr_vertex + 0;
        curr_vertex += 4;
    }

    sg_buffer_desc ibuf_desc = {
        .size = sizeof(indices),
        .usage = {
            .immutable = true,
            .index_buffer = true
        },
        .data = SG_RANGE(indices)
    };
    sg_buffer ibuf = sg_make_buffer(&ibuf_desc);
    renderer->ibuf = ibuf;

    // pipeline
    sg_pipeline_desc pip_desc = {
        // location 0: pos
        .layout.attrs[0] = { .format = SG_VERTEXFORMAT_FLOAT3 },
        // location 1: color
        .layout.attrs[1] = { .format = SG_VERTEXFORMAT_FLOAT4 },
        // location 2: UV
        .layout.attrs[2] = { .format = SG_VERTEXFORMAT_FLOAT2 },
        .index_type = SG_INDEXTYPE_UINT16,
        .shader = shader
    };

    renderer->pip = sg_make_pipeline(&pip_desc);

    // default white texture (solid colors)
    static uint32_t white_pixel = 0xFFFFFFFF; // RGBA8 white (declared static so memory stays persistent during setup)
    renderer->white_image = sg_make_image(&(sg_image_desc){
        .width = 1,
        .height = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
        .data.mip_levels[0] = SG_RANGE(white_pixel)
    });

    renderer->white_tex_view = sg_make_view(&(sg_view_desc){
        .texture.image = renderer->white_image,
        .label = "white-texture-view"
    });

    renderer->default_sampler = sg_make_sampler(&(sg_sampler_desc){ .label = "default-sampler" });
}

void renderer_begin(RenderBatch2d* renderer, Mat4 projection) {
    renderer->vertex_count = 0;
    renderer->projection = projection;
}

// push sprites to GPU and issue draw call
void renderer_flush(RenderBatch2d* renderer) {
    if (renderer->vertex_count == 0) return;

    // append vertices to the buffer tape and retrieve the current byte offset
    int offset = sg_append_buffer(renderer->vbuf, &(sg_range) {
        .ptr = renderer->vertex_buffer,
        .size = sizeof(Vertex) * renderer->vertex_count
    });

    // handle potential frame allocation overflows safely
    if (sg_query_buffer_overflow(renderer->vbuf)) {
        renderer->vertex_count = 0;
        return;
    }

    // bind the buffer and apply the dynamic offset
    sg_bindings bind = {
        .vertex_buffers[0] = renderer->vbuf,
        .vertex_buffer_offsets[0] = offset, // Tracks where this batch begins
        .index_buffer = renderer->ibuf,
        .views[0] = renderer->current_texture_view,
        .samplers[0] = renderer->default_sampler
    };

    // draw using index buffer layout
    sg_apply_pipeline(renderer->pip);
    sg_apply_bindings(&bind);

    // apply projection
    sg_apply_uniforms(0, &SG_RANGE(renderer->projection));

    // each quad has 6 indices.
    // vertex_count / 4 = numer of quads/sprites.
    int num_indices = (renderer->vertex_count / 4) * 6;
    sg_draw(0, num_indices, 1);

    // reset count for the next batch
    renderer->vertex_count = 0;
}

void renderer_push_sprite(RenderBatch2d* renderer, Sprite* sprite) {
    if (renderer->vertex_count + 4 > MAX_VERTICES) {
        renderer_flush(renderer);
    }

    // select texture (or default texture)
    sg_view view = (sprite->texture_view.id != SG_INVALID_ID) ? sprite->texture_view : renderer->white_tex_view;

    // TODO: this is here temporarily, it won't be necessary once sprite manager is introduced
    if (renderer->vertex_count > 0 && renderer->current_texture_view.id != view.id) {
        renderer_flush(renderer);
    }
    renderer->current_texture_view = view;

    int v_idx = renderer->vertex_count;
    sg_color col = sprite->color;

    float half_w = sprite->size.x * 0.5f;
    float half_h = sprite->size.y * 0.5f;

    Vec2 local_corners[4] = {
        { -half_w, -half_h }, // Top-Left
        { -half_w,  half_h }, // Bottom-Left
        {  half_w,  half_h }, // Bottom-Right
        {  half_w, -half_h }  // Top-Right
    };

    Vec2 uv_corners[4] = {
        { 0.0f, 0.0f }, // Top-Left
        { 0.0f, 1.0f }, // Bottom-Left
        { 1.0f, 1.0f }, // Bottom-Right
        { 1.0f, 0.0f }  // Top-Right
    };

    float cos_a = sprite->rotation.x;
    float sin_a = sprite->rotation.y;

    // Rotate + Translate
    for (int i = 0; i < 4; i++) {
        float lx = local_corners[i].x;
        float ly = local_corners[i].y;

        // Rotation matrix math
        float rx = lx * cos_a - ly * sin_a;
        float ry = lx * sin_a + ly * cos_a;

        // Translate to world position
        renderer->vertex_buffer[v_idx + i].position[0] = rx + sprite->position.x;
        renderer->vertex_buffer[v_idx + i].position[1] = ry + sprite->position.y;
        renderer->vertex_buffer[v_idx + i].position[2] = 0.0f; // 2D Z-layer

        // Apply color
        renderer->vertex_buffer[v_idx + i].color[0] = col.r;
        renderer->vertex_buffer[v_idx + i].color[1] = col.g;
        renderer->vertex_buffer[v_idx + i].color[2] = col.b;
        renderer->vertex_buffer[v_idx + i].color[3] = col.a;

        // Add texture UV
        renderer->vertex_buffer[v_idx + i].uv.x = uv_corners[i].x;
        renderer->vertex_buffer[v_idx + i].uv.y = uv_corners[i].y;
    }

    renderer->vertex_count += 4;
}

void renderer_end(RenderBatch2d* renderer) {
    renderer_flush(renderer);
}

void renderer_destroy(RenderBatch2d* renderer) {
    // extra defensive code in case this is called after sokol is destroyed
    if (!sg_isvalid()) return;

    // Safely release the GPU resources
    sg_destroy_buffer(renderer->vbuf);
    sg_destroy_buffer(renderer->ibuf);
    sg_destroy_pipeline(renderer->pip);
    sg_destroy_view(renderer->white_tex_view);
    sg_destroy_image(renderer->white_image);
    sg_destroy_sampler(renderer->default_sampler);

    // Clear handles to prevent accidental use-after-free
    renderer->vbuf.id = 0;
    renderer->ibuf.id = 0;
    renderer->pip.id = 0;
    memset(renderer->vertex_buffer, 0, sizeof(Vertex) * MAX_VERTICES);
    renderer->vertex_count = 0;
}


void sprite_fit_to(Sprite *sprite, RenderTarget *source, RenderTarget *target) {
    float source_aspect = aspect_of(source->size);
    float target_aspect = aspect_of(target->size);

    float draw_w;
    float draw_h;

    if (target_aspect > source_aspect) {
      // pillarbox
      draw_h = target->size.y;
      draw_w = draw_h * source_aspect;
    } else {
      // letterbox
      draw_w = target->size.x;
      draw_h = draw_w / source_aspect;
    }

    float x = (target->size.x - draw_w) * 0.5f;
    float y = (target->size.y - draw_h) * 0.5f;

    sprite->position = (Vec2){ x + draw_w * 0.5f, y + draw_h * 0.5f };
    sprite->size = (Vec2){ draw_w, draw_h };
}

#endif // ENGINE_IMPL_GUARD
#endif // ENGINE_IMPL || __clang_analyzer__
