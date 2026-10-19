#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec4 shade(vec2 p) { return vec4(p.x,p.y,0,1); }
void main() { color=shade(uv); }
