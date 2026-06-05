// fix sokol_time when -std=c99 on Linux
#ifdef __linux__
    #define _POSIX_C_SOURCE 199309L
    #include <time.h>
#endif

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
