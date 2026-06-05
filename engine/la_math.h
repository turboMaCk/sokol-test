#ifndef LA_MATH_H
#define LA_MATH_H

#include <math.h>

// Vec2

typedef struct { float x, y; } Vec2;
static inline Vec2 vec2_add(Vec2 a, Vec2 b);
static inline Vec2 vec2_sub(Vec2 a, Vec2 b);
static inline Vec2 vec2_mul(Vec2 v, float s);
static inline Vec2 vec2_div(Vec2 v, float s);
static inline Vec2 vec2_normalize(Vec2 v);
static inline Vec2 vec2_mul_vec2(Vec2 a, Vec2 b);
static inline Vec2 vec2_div_vec2(Vec2 a, Vec2 b);
static inline Vec2 vec2_min(Vec2 a, Vec2 b);
static inline Vec2 vec2_max(Vec2 a, Vec2 b);
static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t);

// Vec2i

typedef struct { int x, y; } Vec2i;
static inline Vec2 vec2i_to_vec2(Vec2i v);
static inline Vec2i vec2i_add(Vec2i a, Vec2i b);
static inline Vec2i vec2i_sub(Vec2i a, Vec2i b);

// Rot2d

typedef struct { float cos, sin; } Rot2d;
static inline Rot2d rotation_from_rad(float radians);
static inline Rot2d rotation_from_deg(float degrees);
static inline Rot2d rotation_mul(Rot2d a, Rot2d b);
static inline Rot2d rotation_inverse(Rot2d r);

#define ROTATION_NONE      ((Rot2d){  1.0f        ,  0.0f })
#define ROTATION_45        ((Rot2d){  0.70710678f ,  0.70710678f })
#define ROTATION_NEG_45    ((Rot2d){  0.70710678f , -0.70710678f })
#define ROTATION_90        ((Rot2d){  0.0f        ,  1.0f })
#define ROTATION_NEG_90    ((Rot2d){  0.0f        , -1.0f })
#define ROTATION_180       ((Rot2d){ -1.0f        ,  0.0f })
#define ROTATION_NEG_180   ((Rot2d){ -1.0f        ,  0.0f })

// Mat4

// Note we're using column major matrixes
// to be consistent with OpenGL
typedef struct { float m[16]; } Mat4;
Mat4 mat4_mul(Mat4 a, Mat4 b);
Mat4 mat4_ortho(float left, float right, float top, float bottom);

#endif // LA_MATH_H
#if defined(ENGINE_IMPL) || defined(__CLANGD__)

#ifndef ENGINE_LA_MATH_IMPL_GUARD
#define ENGINE_LA_MATH_IMPL_GUARD

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

static inline Vec2 vec2_mul(Vec2 v, float s) {
    return (Vec2) {
        .x = v.x * s,
        .y = v.y * s,
    };
}

static inline Vec2 vec2_div(Vec2 v, float s) {
    return (Vec2) {
        .x = v.x / s,
        .y = v.y / s,
    };
}

static inline Vec2 vec2i_to_vec2(Vec2i v) {
    return (Vec2){
        .x = (float)v.x,
        .y = (float)v.y,
    };
}

static inline Vec2 vec2_mul_vec2(Vec2 a, Vec2 b) {
    return (Vec2){
        .x = a.x * b.x,
        .y = a.y * b.y,
    };
}

static inline Vec2 vec2_div_vec2(Vec2 a, Vec2 b) {
    return (Vec2){
        .x = a.x / b.x,
        .y = a.y / b.y,
    };
}

static inline Vec2 vec2_min(Vec2 a, Vec2 b) {
    return (Vec2) {
        .x = (a.x < b.x) ? a.x : b.x,
        .y = (a.y < b.y) ? a.y : b.y,
    };
}

static inline Vec2 vec2_max(Vec2 a, Vec2 b) {
    return (Vec2) {
        .x = (a.x > b.x) ? a.x : b.x,
        .y = (a.y > b.y) ? a.y : b.y,
    };
}

static inline Vec2 vec2_normalize(Vec2 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len == 0.0f) return (Vec2){0};
    return (Vec2) {
        .x = v.x / len,
        .y = v.y / len,
    };
}

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
    return (Vec2) {
        .x = a.x + (b.x - a.x) * t,
        .y = a.y + (b.y - a.y) * t,
    };
}

static inline Vec2i vec2i_add(Vec2i a, Vec2i b) {
    return (Vec2i){
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

static inline Vec2i vec2i_sub(Vec2i a, Vec2i b) {
    return (Vec2i){
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

Rot2d rotation_from_rad(float radians) {
    return (Rot2d) {
        .cos = cosf(radians),
        .sin = sinf(radians),
    };
}

static inline Rot2d rotation_from_deg(float degrees) {
    // Convert degrees to radians: (deg * PI / 180)
    // Using 0.0174532925f (PI / 180) avoids a runtime division
    float radians = degrees * 0.0174532925f;
    return rotation_from_rad(radians);
}

// Multiplication of 2 complex numbers is like angle addition
static inline Rot2d rotation_mul(Rot2d a, Rot2d b) {
    return (Rot2d){
        .cos = a.cos * b.cos - a.sin * b.sin,
        .sin = a.sin * b.cos + a.cos * b.sin,
    };
}

static inline Rot2d rotation_inverse(Rot2d r) {
    return (Rot2d) {
        .cos = r.cos,
        .sin = -r.sin
    };
}

Mat4 mat4_ortho(float left, float right, float top, float bottom) {
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

Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 r = {0};

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }

    return r;
}

#endif // ENGINE_LA_MATH_IMPL_GUARD
#endif // ENGINE_IMPL || __clang_analyzer__
