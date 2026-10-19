#version 450
layout(location=0) in vec3 position;
void main() { float halfW=0.5; float w=halfW+halfW; gl_Position=vec4(position,w); }
