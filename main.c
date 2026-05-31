#define ENGINE_IMPL
#include "engine/renderer.h"

#define SOKOL_IMPL
#define SOKOL_GLCORE33

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#include "shaders/sprite.h"

#define GAME_TARGET_WIDTH 320.0f
#define GAME_TARGET_HEIGHT 180.0f
#define GAME_ASPECT (GAME_TARGET_WIDTH / GAME_TARGET_HEIGHT)

#define WIN_WIDTH 1280
#define WIN_HEIGHT 720

static RenderBatch2d renderer;
static RenderTarget game_target;
static RenderTarget screen_target;

void init(void) {
    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func
    };
    sg_setup(&desc);
    sg_shader shader = sg_make_shader(sprite2d_shader_desc(sg_query_backend()));

    renderer_init(&renderer, shader);

    // Both point to the same texture canvas
    game_target = render_target_create_offscreen(GAME_TARGET_WIDTH, GAME_TARGET_HEIGHT);

    screen_target = render_target_create_swapchain();
}

void frame(void) {
    Sprite game_sprite = {
        .position = {0.0f, 0.0f},
        .size = {0.5f, 0.5f},
        .rotation = rotation_from_deg(45.0f),
        .color = {1.0f, 0.0f, 0.0f, 1.0f} // Red color
    };

    Sprite hud_sprite = {
        .position = {20.0f, 20.0f},       // 20 pixels in from top-left of the game canvas
        .size = {300.0f, 40.0f},          // A long health-bar style rectangle
        .rotation = rotation_from_deg(0.0f),
        .color = {0.0f, 1.0f, 0.0f, 1.0f} // Green color
    };

    Sprite editor_pane_rect = {
        .rotation = ROTATION_NONE,
        .color = {1,1,1,1},
        .texture_view = game_target.color_texture_view
    };

    sprite_fit_to(&editor_pane_rect, &game_target, &screen_target);

    // -------------------------------------------------------------
    // PASS 1: GAMEPLAY LAYERS (Offscreen Target)
    // -------------------------------------------------------------
    renderer_begin_target_pass(&game_target, (sg_color){ 0.2f, 0.3f, 0.4f, 1.0f });
    {
        // Draw World Space (Red rotating box in the middle)
        Mat4 game_proj = mat4_ortho(-GAME_ASPECT, GAME_ASPECT, -1.0f, 1.0f);
        renderer_begin(&renderer, game_proj);
        renderer_push_sprite(&renderer, &game_sprite);
        renderer_end(&renderer);

        // Draw Game HUD Space (Green status bar anchored to virtual top-left)
        Mat4 game_ui_proj = mat4_ortho(0.0f, game_target.size.x, game_target.size.y, 0.0f);
        renderer_begin(&renderer, game_ui_proj);
        renderer_push_sprite(&renderer, &hud_sprite);
        renderer_end(&renderer);
    }
    renderer_end_target_pass(&game_target);

    // -------------------------------------------------------------
    // PASS 2: APP ENVIRONMENT LAYERS (Screen Target)
    // -------------------------------------------------------------
    renderer_begin_target_pass(&screen_target, (sg_color){ 0.1f, 0.1f, 0.1f, 1.0f });
    {
        // Draw Editor Panels / Clay Layout Viewports
        Mat4 native_proj = mat4_ortho(0.0f, screen_target.size.x, screen_target.size.y, 0.0f);
        renderer_begin(&renderer, native_proj);

        // Draw our placeholder panel rect where the game texture will now clip in
        renderer_push_sprite(&renderer, &editor_pane_rect);

        renderer_end(&renderer);
    }
    renderer_end_target_pass(&screen_target);

    sg_commit();
}

void cleanup(void) {
    renderer_destroy(&renderer);
    render_target_destroy(&game_target);
    render_target_destroy(&screen_target);
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char **argv) {
    (void)argc; (void)argv;

    sapp_desc desc = {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = WIN_WIDTH,
        .height = WIN_HEIGHT,
        .window_title = "Sokol",
        .logger.func = slog_func
    };
    return desc;
}
