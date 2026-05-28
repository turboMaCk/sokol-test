#define ENGINE_IMPL
#include "engine/renderer.h"

#define SOKOL_IMPL
#define SOKOL_GLCORE33

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"


static RenderBatch2d renderer;

void init(void) {
    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func
    };
    sg_setup(&desc);

    sg_shader_desc shader_desc = {
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
            "  f_color = v_color;\n" // Use color passed from vertices
            "}"
    };
    sg_shader shader = sg_make_shader(&shader_desc);

    renderer_init(&renderer, shader);
}

void frame(void) {
    sg_pass_action pass_action = {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.1f, 0.1f, 0.1f, 1.0f }
        }
    };

    sg_begin_pass(&(sg_pass){
        .action = pass_action,
        .swapchain = sglue_swapchain()
    });

    renderer_begin(&renderer);
    {
        Sprite sprite = {
            .position = {0.0f, 0.0f},
            .size = {0.5f, 0.5f},
            .rotation = 0.0f,
            .color = {1.0f, 0.0f, 0.0f, 1.0f} // Red color
        };
        renderer_push_sprite(&renderer, &sprite);
    }
    renderer_end(&renderer);

    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    renderer_destroy(&renderer);
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char **argv) {
    (void)argc; (void)argv
    ;
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
