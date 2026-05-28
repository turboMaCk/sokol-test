#define SOKOL_IMPL
#define SOKOL_GLCORE33

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

typedef struct {
    float position[3];
    float color[4];
} Vertex;

static sg_buffer vbuf;
static sg_pipeline pip;

void init(void) {
    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func
    };
    sg_setup(&desc);

    Vertex vertices[] = {
        { { +0.0f, +0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { +0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };

    sg_buffer_desc buf_desc = {
        .data = SG_RANGE(vertices)
    };
    vbuf = sg_make_buffer(&buf_desc);

    // Fixed layout for modern sg_shader_desc structure
    sg_shader_desc shd_desc = {
        .vertex_func.source =
            "#version 330 core\n"
            "layout(location=0) in vec3 a_position;\n"
            "layout(location=1) in vec4 a_color;\n"
            "out vec4 v_color;\n"
            "void main() {\n"
            "  gl_Position = vec4(a_position, 1.0);\n"
            "  v_color = a_color;\n"
            "}",
        .fragment_func.source =
            "#version 330 core\n"
            "in vec4 v_color;\n"
            "out vec4 f_color;\n"
            "void main() {\n"
            "  f_color = v_color;\n"
            "}"
    };
    sg_shader shd = sg_make_shader(&shd_desc);

    sg_pipeline_desc pip_desc = {
        .shader = shd,
        .layout = {
            .attrs[0].format = SG_VERTEXFORMAT_FLOAT3,
            .attrs[1].format = SG_VERTEXFORMAT_FLOAT4
        }
    };
    pip = sg_make_pipeline(&pip_desc);
}

void frame(void) {
    // Fixed pass initialization using proper sglue_swapchain() call
    sg_pass pass = {
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.5f, 0.5f, 0.5f, 1.0f }
            }
        },
        .swapchain = sglue_swapchain()
    };
    sg_begin_pass(&pass);

    sg_bindings bind = {
        .vertex_buffers[0] = vbuf
    };

    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);

    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char **argv) {
    (void)argc; (void)argv;
    sapp_desc desc = {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "Sokol",
        .logger.func = slog_func
    };
    return desc;
}
