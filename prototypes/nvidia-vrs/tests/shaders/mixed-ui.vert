#version 450
layout(location=0) in vec3 position;
void main() { gl_Position=vec4(position,position.x>0 ? 1.0 : 2.0); }
