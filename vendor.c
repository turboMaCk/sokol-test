#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SOKOL_IMPL
#define SOKOL_DLL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_time.h"

extern sapp_desc sokol_main(int argc, char **argv);
