#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(set=0,binding=0) buffer Result { float count[]; } result;
void main() { result.count[int(uv.x*8)]=uv.y; color=vec4(uv,0,1); }
