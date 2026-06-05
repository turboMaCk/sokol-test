#ifndef RENDERER_H
#define RENDERER_H

#include "sokol_gfx.h"
#include "la_math.h"
#include "assets.h"

#ifndef RENDERER_MAX_SPRITES
#define RENDERER_MAX_SPRITES 2048
#endif
#define MAX_VERTICES (RENDERER_MAX_SPRITES * 4) // 4 vertices per quad
#define MAX_INDICES (RENDERER_MAX_SPRITES * 6) // 6 indices per quad

// Camera

typedef struct {
    Vec2 position;
    float zoom;
    float follow_speed;

    // shake
    float shake_strength;   // max offset in pixels
    float shake_decay;      // how fast it fades
    float shake_time;       // internal timer
} Camera2d;

static inline Camera2d camera2d_default(void);
Mat4 camera2d_view_proj(Camera2d cam, Vec2i target_size);
void camera2d_shake(Camera2d* cam, float strength);
void camera2d_update(Camera2d* cam, float dt);
void camera2d_follow(Camera2d* cam, Vec2 position, float dt);

// Texture
// layout: [ kind:3 | size:5 ]
#define PIXEL_SIZE_BITS 5
#define PIXEL_SIZE_MASK ((1u<<PIXEL_SIZE_BITS)-1)


// forward declared
typedef struct SpriteImage SpriteImage;

SpriteImage texture_atlas_region(TextureAtlas* atlas, Vec2i position, Vec2i size);

// Renderer

typedef struct {
    sg_sampler sampler;
} Sampler;

Sampler sampler_create_default(void);
void sampler_destroy(Sampler* sampler);

typedef enum {
    RenderTargetOffscreen = 0, // default value
    RenderTargetSwapchain = 1,
} RenderTargetKind;

typedef struct {
    RenderTargetKind kind;
    Vec2i size;
    // only off screen render target defines these
    sg_image color_img;
    sg_image depth_img;
    sg_view color_view;
    sg_view depth_view;
    sg_view color_texture_view;
} RenderTarget;

RenderTarget render_target_create_offscreen(int width, int height);
RenderTarget render_target_create_swapchain(void);
void render_target_destroy(RenderTarget* target);
void renderer_begin_target_pass(RenderTarget* target, sg_color clear_color);
void renderer_end_target_pass();

typedef struct {
    float position[3];
    float color[4];
    Vec2 uv;
} Vertex;

struct SpriteImage {
    sg_view texture_view;
    Vec2 uv_start;
    Vec2 uv_end;
};

SpriteImage sprite_image_from_texture(Texture* texture);
SpriteImage sprite_image_from_texture_region(Texture* texture, Vec2i position, Vec2i size);
SpriteImage sprite_image_from_render_target(RenderTarget* render_target);

typedef struct {
    Vec2 position;
    Vec2 size;
    Rot2d rotation;
    sg_color color;
    SpriteImage image;
} Sprite;

typedef struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pipeline pip;

    Vertex vertex_buffer[MAX_VERTICES];
    int vertex_count;

    // current projection, applied during renderer_begin()
    Mat4 projection;

    // The persistent sampler initialized once during setup
    sg_sampler default_sampler;

    // TODO: these live in here temporarily
    // they will be moved to sprite manager
    sg_view current_texture_view;
} RenderBatch2d;

void renderer_init(RenderBatch2d* renderer, sg_shader shader);
void renderer_begin(RenderBatch2d* renderer, Mat4 projection);
void renderer_flush(RenderBatch2d* renderer);
void renderer_end(RenderBatch2d* renderer);
void renderer_push_sprite(RenderBatch2d* renderer, Sprite* sprite);
void renderer_destroy(RenderBatch2d* renderer);
// TODO: this name is not ideal
void sprite_fit_to(Sprite* sprite, RenderTarget* source, RenderTarget* target);

#endif // RENDERER_H

#if defined(ENGINE_IMPL) || defined(__CLANGD__)

#ifndef ENGINE_RENDERER_IMPL_GUARD
#define ENGINE_RENDERER_IMPL_GUARD

#include <string.h>
#include <math.h>
#include "sokol_app.h"
#include "stb_image.h"

// Camera

static inline Camera2d camera2d_default(void) {
    return (Camera2d) {
        .position = {0,0},
        .zoom = 1.0f,
        .follow_speed = 10.0f,
        .shake_strength = 0.0f,
        .shake_decay = 5.0f,
        .shake_time = 0.0f,
    };
}

static inline Vec2 camera2d_get_shake_offset(Camera2d* cam) {
    if (cam->shake_strength <= 0.0f) {
        return (Vec2){0,0};
    }

    // simple deterministic noise based on time
    float t = cam->shake_time * 60.0f;

    float x = sinf(t * 12.9898f) * 43758.5453f;
    float y = sinf(t * 78.233f)  * 12345.6789f;

    float nx = (x - floorf(x)) * 2.0f - 1.0f;
    float ny = (y - floorf(y)) * 2.0f - 1.0f;

    return (Vec2){
        .x = nx * cam->shake_strength,
        .y = ny * cam->shake_strength
    };
}

Mat4 camera2d_view_proj(Camera2d cam, Vec2i target_size) {
    float w = (float)target_size.x;
    float h = (float)target_size.y;

    // 0,0 in top left corner
    Mat4 proj = mat4_ortho(0, w, 0, h);

    // shake
    Vec2 shake = camera2d_get_shake_offset(&cam);

    // zoom
    float sx = cam.zoom;
    float sy = cam.zoom;

    // could use identity constructior
    // and set just 0 and 5
    Mat4 view = {0};
    view.m[0] = sx;
    view.m[5] = sy;
    view.m[10] = 1.0f;
    view.m[15] = 1.0f;

    // move world opossite to the camera position
    view.m[12] = -(cam.position.x + shake.x) * sx + target_size.x * 0.5;
    view.m[13] = -(cam.position.y - shake.y) * sy + target_size.y * 0.5;

    return mat4_mul(proj, view);
}

// strength is in pixels
void camera2d_shake(Camera2d* cam, float strength) {
    if (strength > cam->shake_strength) {
        cam->shake_strength = strength;
    }
    cam->shake_time = 0.0f;
}

void camera2d_update(Camera2d* cam, float dt) {
    cam->shake_time += dt;

    // exponential decay
    cam->shake_strength *= expf(-cam->shake_decay * dt);

    if (cam->shake_decay < 0.01f) {
        cam->shake_strength = 0.0f;
    }
}

void camera2d_follow(Camera2d* cam, Vec2 target, float dt) {
    float t = 1.0f - expf(-cam->follow_speed * dt);
    cam->position = vec2_lerp(cam->position, target, t);
}

static Texture* texture_white_pixel(void) {
    static uint32_t white_pixel_data = 0xFFFFFFFF;
    static Texture white_pixel;
    static bool white_pixel_initialized = false;

    if (!white_pixel_initialized) {
        white_pixel = texture_create_rgba((Vec2i){1,1}, &white_pixel_data);
        white_pixel_initialized = true;
    }

    return &white_pixel;
}

SpriteImage texture_atlas_region(TextureAtlas* atlas, Vec2i position, Vec2i size) {
    return sprite_image_from_texture_region(&atlas->texture, position, size);
}

// Renderer

Sampler sampler_create_default(void) {
    sg_sampler sampler = sg_make_sampler(&(sg_sampler_desc){ .label = "default-sampler" });

    return (Sampler){sampler};
}

void sampler_destroy(Sampler* sampler) {
    sg_destroy_sampler(sampler->sampler);
    memset(sampler, 0, sizeof(Sampler));
}

// RenderTarget

RenderTarget render_target_create_offscreen(int width, int height) {
    RenderTarget target = {
        .kind = RenderTargetOffscreen,
        .size = (Vec2i) {width, height},
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
    return (RenderTarget) {
        .kind = RenderTargetSwapchain,
        .size = (Vec2i){0},   // Will be overwritten with actual window width on frame 1
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

    switch (target->kind) {
    case RenderTargetSwapchain: {
        extern sg_swapchain sglue_swapchain(void);

        // Keep our internal engine tracking dimensions updated in case of resize
        target->size.x = sapp_width();
        target->size.y = sapp_height();

        // Let the glue library completely manage the swapchain population
        pass.swapchain = sglue_swapchain();
    } break;
    case RenderTargetOffscreen: {
        // Direct view assignments for offscreen rendering
        pass.attachments.colors[0] = target->color_view;
        pass.attachments.depth_stencil = target->depth_view;
    } break;
    }

    sg_begin_pass(&pass);
}

void renderer_end_target_pass() {
    sg_end_pass();
}

void render_target_destroy(RenderTarget* target) {
    if (target->kind == RenderTargetSwapchain) return;
    sg_destroy_image(target->color_img);
    sg_destroy_image(target->depth_img);
    sg_destroy_view(target->color_view);
    sg_destroy_view(target->depth_view);
    sg_destroy_view(target->color_texture_view);
    target->size = (Vec2i){0};
    target->kind = RenderTargetSwapchain;
}

static inline float aspect_of(Vec2i rect) {
    return (float)rect.x / (float)rect.y;
}

SpriteImage sprite_image_from_texture(Texture* texture) {
    return (SpriteImage) {
        .texture_view = texture->view,
        .uv_start = (Vec2){0,0},
        .uv_end = (Vec2){1,1},
    };
}

SpriteImage sprite_image_from_texture_region(Texture* texture, Vec2i position, Vec2i size) {
    Vec2 tex_size = vec2i_to_vec2(texture->size);

    return (SpriteImage){
        .texture_view = texture->view,
        .uv_start = vec2_div_vec2(vec2i_to_vec2(position), tex_size),
        .uv_end = vec2_div_vec2(
            vec2i_to_vec2(vec2i_add(position, size)),
            tex_size
        ),
    };
}

SpriteImage sprite_image_from_render_target(RenderTarget* render_target) {
    return (SpriteImage){
        .texture_view = render_target->color_texture_view,
        .uv_start = (Vec2){0,0},
        .uv_end = (Vec2){1,1},
    };
}

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
    uint16_t indices[MAX_INDICES];
    int curr_vertex = 0;
    for (int i = 0; i < (MAX_INDICES); i += 6) {
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
        .layout.attrs[0] = {.format = SG_VERTEXFORMAT_FLOAT3},
        // location 1: color
        .layout.attrs[1] = {.format = SG_VERTEXFORMAT_FLOAT4},
        // location 2: UV
        .layout.attrs[2] = {.format = SG_VERTEXFORMAT_FLOAT2},
        .index_type = SG_INDEXTYPE_UINT16,
        .shader = shader,
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            },
        },
    };

    renderer->pip = sg_make_pipeline(&pip_desc);

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
    sg_view view = sprite->image.texture_view.id != SG_INVALID_ID ? sprite->image.texture_view : texture_white_pixel()->view;

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
        { sprite->image.uv_start.x, sprite->image.uv_start.y },
        { sprite->image.uv_start.x, sprite->image.uv_end.y   },
        { sprite->image.uv_end.x,   sprite->image.uv_end.y   },
        { sprite->image.uv_end.x,   sprite->image.uv_start.y }
    };

    float cos_a = sprite->rotation.cos;
    float sin_a = sprite->rotation.sin;

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
    sg_destroy_sampler(renderer->default_sampler);

    // Clear handles to prevent accidental use-after-free
    renderer->vbuf.id = 0;
    renderer->ibuf.id = 0;
    renderer->pip.id = 0;
    memset(renderer->vertex_buffer, 0, sizeof(Vertex) * MAX_VERTICES);
    renderer->vertex_count = 0;
}

void sprite_fit_to(Sprite* sprite, RenderTarget* source, RenderTarget* target) {
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

#endif // ENGINE_RENDERER_IMPL_GUARD
#endif // ENGINE_IMPL || __clang_analyzer__
