#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() { gl_FragDepth=uv.x; color=vec4(uv,0,1); }
