#ifndef RENDERER_H
#define RENDERER_H

#include "sokol_gfx.h"

#define MAX_SPRITES 2048
#define MAX_VERTICES (MAX_SPRITES * 4) // 4 vertices per quad
#define MAX_INDICES (MAX_SPRITES * 6) // 6 indices per quad

// TODO: probably move to math module
typedef struct { float x, y; } Vec2;
typedef struct { float r, g, b, a; } Rgba;

typedef struct {
    float position[3];
    float color[4];
} Vertex;

typedef struct {
    sg_buffer vbuf;
    sg_buffer ibuf;
    sg_pipeline pip;

    Vertex vertex_buffer[MAX_VERTICES];
    int vertex_count;

    // Track current state to know when to flush
    uint32_t current_texture_id;
} RenderBatch2d;

typedef struct {
    Vec2 position;
    Vec2 size;
    float rotation; // In radians
    Rgba color;
    // uint32_t texture_id; // Ready for later
} Sprite;

void renderer_init(RenderBatch2d* batcher, sg_shader shader);
void renderer_begin(RenderBatch2d* batcher);
void renderer_flush(RenderBatch2d* batcher);
void renderer_end(RenderBatch2d* batcher);
void renderer_push_sprite(RenderBatch2d* batcher, Sprite* sprite);
void renderer_destroy(RenderBatch2d* batcher);

#endif // RENDERER_H

#if defined(ENGINE_IMPL) || defined(__CLANGD__)

#ifndef ENGINE_IMPL_GUARD
#define ENGINE_IMPL_GUARD

#include <string.h>
#include <math.h>

void renderer_init(RenderBatch2d* batcher, sg_shader shader) {
    memset(batcher, 0, sizeof(RenderBatch2d));

    // vertex buffer
    sg_buffer_desc vbuf_desc = {
        .size = sizeof(Vertex) * MAX_VERTICES,
        .usage = {
            .dynamic_update = true,
            .vertex_buffer = true
        }
    };

    sg_buffer vbuf = sg_make_buffer(&vbuf_desc);
    batcher->vbuf = vbuf;

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
    batcher->ibuf = ibuf;

    // pipeline
    sg_pipeline_desc pip_desc = {
        .shader = shader,
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT3, // position
            .attrs[1].format = SG_VERTEXFORMAT_FLOAT4  // color
        },
        .index_type = SG_INDEXTYPE_UINT16
    };

    batcher->pip = sg_make_pipeline(&pip_desc);
}

void renderer_begin(RenderBatch2d* batcher) {
    batcher->vertex_count = 0;
    batcher->current_texture_id = 0; // 0 means no texture bound yet
}

// push sprites to GPU and issue draw call
void renderer_flush(RenderBatch2d* batcher) {
    if (batcher->vertex_count == 0) return;

    // append vertices to the buffer tape and retrieve the current byte offset
    int offset = sg_append_buffer(batcher->vbuf, &(sg_range) {
        .ptr = batcher->vertex_buffer,
        .size = sizeof(Vertex) * batcher->vertex_count
    });

    // handle potential frame allocation overflows safely
    if (sg_query_buffer_overflow(batcher->vbuf)) {
        batcher->vertex_count = 0;
        return;
    }

    // bind the buffer and apply the dynamic offset
    sg_bindings bind = {
        .vertex_buffers[0] = batcher->vbuf,
        .vertex_buffer_offsets[0] = offset, // Tracks where this batch begins
        .index_buffer = batcher->ibuf
    };

    // draw using index buffer layout
    sg_apply_pipeline(batcher->pip);
    sg_apply_bindings(&bind);

    // each quad has 6 indices.
    // vertex_count / 4 = numer of quads/sprites.
    int num_indices = (batcher->vertex_count / 4) * 6;
    sg_draw(0, num_indices, 1);

    // reset count for the next batch
    batcher->vertex_count = 0;
}

void renderer_push_sprite(RenderBatch2d* batcher, Sprite* sprite) {
    if (batcher->vertex_count + 4 > MAX_VERTICES) {
        renderer_flush(batcher);
    }

    int v_idx = batcher->vertex_count;
    Rgba col = sprite->color;

    float half_w = sprite->size.x * 0.5f;
    float half_h = sprite->size.y * 0.5f;

    Vec2 local_corners[4] = {
        { -half_w, -half_h }, // Top-Left
        { -half_w,  half_h }, // Bottom-Left
        {  half_w,  half_h }, // Bottom-Right
        {  half_w, -half_h }  // Top-Right
    };

    // TODO: maybe we should cache results of these
    // calls so we can reuse already calculated ones across
    // all sprites
    float cos_a = cosf(sprite->rotation);
    float sin_a = sinf(sprite->rotation);

    // Rotate + Translate
    for (int i = 0; i < 4; i++) {
        float lx = local_corners[i].x;
        float ly = local_corners[i].y;

        // Rotation matrix math
        float rx = lx * cos_a - ly * sin_a;
        float ry = lx * sin_a + ly * cos_a;

        // Translate to world position
        batcher->vertex_buffer[v_idx + i].position[0] = rx + sprite->position.x;
        batcher->vertex_buffer[v_idx + i].position[1] = ry + sprite->position.y;
        batcher->vertex_buffer[v_idx + i].position[2] = 0.0f; // 2D Z-layer

        // Apply color
        batcher->vertex_buffer[v_idx + i].color[0] = col.r;
        batcher->vertex_buffer[v_idx + i].color[1] = col.g;
        batcher->vertex_buffer[v_idx + i].color[2] = col.b;
        batcher->vertex_buffer[v_idx + i].color[3] = col.a;
    }

    batcher->vertex_count += 4;
}

void renderer_end(RenderBatch2d* batcher) {
    renderer_flush(batcher);
}

void renderer_destroy(RenderBatch2d* batcher) {
    // Safely release the GPU resources
    sg_destroy_buffer(batcher->vbuf);
    sg_destroy_buffer(batcher->ibuf);
    sg_destroy_pipeline(batcher->pip);

    // Clear handles to prevent accidental use-after-free
    batcher->vbuf.id = 0;
    batcher->ibuf.id = 0;
    batcher->pip.id = 0;
    batcher->vertex_count = 0;
}

#endif // ENGINE_IMPL_GUARD
#endif // ENGINE_IMPL || __clang_analyzer__
