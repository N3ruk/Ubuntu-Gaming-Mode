#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(set=0,binding=0) buffer Result { uint count; } result;
void main() { atomicAdd(result.count,1); color=vec4(uv,0,1); }
