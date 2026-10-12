#version 450
layout(location=0) in vec3 position;
void main() { vec4 p = vec4(position, 1.0); gl_Position = p; }
