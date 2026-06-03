#define ENGINE_IMPL
#include "engine/renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

static TextureAtlas orc_atlas;

static Rot2d rotation = ROTATION_NONE;
static Rot2d rotation_delta;
static Vec2 p_pos = {0};

void init(void) {
    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func
    };
    sg_setup(&desc);
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

    // game stuff
    rotation_delta = rotation_from_deg(1.0f);
    p_pos = (Vec2){(float)game_target.size.x/2, (float)game_target.size.y};
}

void frame(void) {
    // projections
    Mat4 game_proj = mat4_ortho(0, game_target.size.x, 0, game_target.size.y);
    Mat4 native_proj = mat4_ortho(0.0f, screen_target.size.x, screen_target.size.y, 0.0f);

    rotation = rotation_mul(rotation, rotation_delta);
    p_pos.y-= .5;

    Vec2 center = vec2_div(vec2i_to_vec2(game_target.size), 2);

    Sprite game_sprite = {
        .position = center,
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
        .position = {(float)game_target.size.x/2, 0},       // 20 pixels in from top-left of the game canvas
        .size = {(float)game_target.size.x, 28},          // A long health-bar style rectangle
        .rotation = ROTATION_NONE,
        .color = {0.0f, 1.0f, 0.0f, 1.0f} // Green color
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
        renderer_begin(&renderer, game_proj);
        {
          renderer_push_sprite(&renderer, &game_sprite);
          renderer_push_sprite(&renderer, &orc_sprite);
        }
        renderer_end(&renderer);

        renderer_begin(&renderer, game_proj); {
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
