#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() { if (uv.x < 0.5) discard; color = vec4(uv,0,1); }
