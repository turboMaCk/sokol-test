@vs vs
layout(location=0) in vec3 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec2 a_uv;
layout(binding=0) uniform vs_params {
    mat4 u_projection;
};

out vec4 v_color;
out vec2 v_uv;

void main() {
    gl_Position = u_projection * vec4(a_position, 1.0);
    v_color = a_color;
    v_uv = a_uv;
}
@end

@fs fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec4 v_color;
in vec2 v_uv;

out vec4 f_color;

void main() {
    f_color = texture(sampler2D(tex, smp), v_uv) * v_color;
}
@end

@program sprite2d vs fs
