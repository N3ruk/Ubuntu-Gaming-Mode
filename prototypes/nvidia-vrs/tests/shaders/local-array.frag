#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() { float values[2]; values[0]=uv.x; values[1]=uv.y;
 values[int(uv.x)&1]=uv.x+uv.y; color=vec4(values[0],values[1],0,1); }
