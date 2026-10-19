#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(std430,set=0,binding=0) buffer Storage { float values[]; } data;
void main() { data.values[uint(uv.x)]=uv.y; color=vec4(uv,0,1); }
