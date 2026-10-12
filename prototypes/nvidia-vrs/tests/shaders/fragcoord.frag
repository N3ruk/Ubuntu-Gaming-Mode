#version 450
layout(location=0) out vec4 color;
void main() {
    float c = mod(floor(gl_FragCoord.x) + floor(gl_FragCoord.y), 2.0);
    color = vec4(c,c,c,1.0);
}
