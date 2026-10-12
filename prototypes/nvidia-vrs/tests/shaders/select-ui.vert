#version 450
layout(location=0) in vec3 position;
void main() {
    vec4 a = vec4(position, 1.0);
    vec4 b = vec4(position * 2.0, 1.0);
    gl_Position = position.x > 0.0 ? a : b;
}
