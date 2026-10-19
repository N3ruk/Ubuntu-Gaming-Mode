#version 450
#extension GL_EXT_demote_to_helper_invocation : require
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main() { if(uv.x<0.5) demote; color=vec4(uv,0,1); }
