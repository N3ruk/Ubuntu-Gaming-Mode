#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() {
    float c = mod(floor(uv.x * 64.0) + floor(uv.y * 64.0), 2.0);
    color = vec4(c,c,c,1.0);
}
