#include <assert.h>

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_time.h"

#define ENGINE_IMPL
#include "engine/renderer.h"
#include "shaders/sprite.h"

#define GAME_TARGET_WIDTH 320.0f
#define GAME_TARGET_HEIGHT 180.0f
#define GAME_ASPECT (GAME_TARGET_WIDTH / GAME_TARGET_HEIGHT)

#define WIN_WIDTH 1280
#define WIN_HEIGHT 720

#define clamp(a, max) (a > max ? max : a)

static RenderBatch2d renderer;
static RenderTarget game_target;
static RenderTarget screen_target;

static TextureAtlas orc_atlas;

static Rot2d rotation = ROTATION_NONE;
static Rot2d rotation_delta;
static Vec2 p_pos = {0};

static Camera2d game_cam;

uint64_t last_time;

void init(void) {
    // sokol init
    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func
    };
    sg_setup(&desc);
    stm_setup(); // setup time tracking
    last_time = stm_now();

    sg_shader shader = sg_make_shader(sprite2d_shader_desc(sg_query_backend()));

    // renderer
    renderer_init(&renderer, shader);

    // render targets
    game_target = render_target_create_offscreen(GAME_TARGET_WIDTH, GAME_TARGET_HEIGHT);
    screen_target = render_target_create_swapchain();

    // texture atlas
    Texture orc_texture = texture_load_png("./resources/orc.png");
    assert(texture_valid(&orc_texture));
    orc_atlas = texture_atlas_from_texture(orc_texture);

    // camera
    game_cam = camera2d_default();
    game_cam.position.y = -GAME_TARGET_HEIGHT;
    camera2d_shake(&game_cam, 10.0f);

    // game stuff
    p_pos = (Vec2){0,0};
}

void frame(void) {
    // keep track of timing
    uint64_t now = stm_now();
    // clamp to prevent large time deltas (e.g. when debugging with breakpoints)
    double dt = clamp(stm_sec(stm_diff(now, last_time)), 0.1);
    last_time = now;

    camera2d_follow(&game_cam, p_pos, dt);
    camera2d_update(&game_cam, dt);

    rotation_delta = rotation_from_deg(1.0f);
    rotation = rotation_mul(rotation, rotation_delta);
    Sprite game_sprite = {
        .position = {0,0},
        .size = {100, 100},
        .rotation = rotation,
        .color = {1.0f, 0.0f, 0.0f, 1.0f}, // Red color
    };

    Sprite orc_sprite = {
        .position = p_pos,
        .size = {28, 28},
        .rotation = ROTATION_NONE,
        .image = sprite_image_from_texture_region(&orc_atlas.texture, (Vec2i){41,37}, (Vec2i){28,28}),
        .color = {1,1,1,1},
    };

    Sprite hud_sprite = {
        .position = {(float)game_target.size.x/2, 0}, // 20 pixels in from top-left of the game canvas
        .size = {(float)game_target.size.x, 28},      // A long health-bar style rectangle
        .rotation = ROTATION_NONE,
        .color = {0.0f, 1.0f, 0.0f, 1.0f}             // Green color
    };

    Sprite editor_pane_rect = {
        .rotation = ROTATION_NONE,
        .color = {1,1,1,1},
        .image = sprite_image_from_render_target(&game_target),
    };

    sprite_fit_to(&editor_pane_rect, &game_target, &screen_target);

    // -------------------------------------------------------------
    // PASS 1: GAMEPLAY LAYERS (Offscreen Target)
    // -------------------------------------------------------------
    renderer_begin_target_pass(&game_target, (sg_color){ 0.2f, 0.3f, 0.4f, 1.0f });
    {
        Mat4 game_proj = camera2d_view_proj(game_cam, game_target.size);
        renderer_begin(&renderer, game_proj);
        {
          renderer_push_sprite(&renderer, &game_sprite);
          renderer_push_sprite(&renderer, &orc_sprite);
        }
        renderer_end(&renderer);

        Mat4 game_ui_proj = mat4_ortho(0, game_target.size.x, 0, game_target.size.y);
        renderer_begin(&renderer, game_ui_proj); {
            renderer_push_sprite(&renderer, &hud_sprite);
        }
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
        {
            renderer_push_sprite(&renderer, &editor_pane_rect);
        }

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
