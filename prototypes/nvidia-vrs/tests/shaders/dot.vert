#version 450
layout(location=0) in vec3 position;
layout(set=0,binding=0) uniform Camera { vec4 row; } camera;
void main() { gl_Position=vec4(position,dot(camera.row,vec4(position,1))); }
