#version 330 core
layout (location = 0) in vec2 pos;

void main() {
	float aspectRatio = 1280.0 / 720.0;
	gl_Position = vec4(vec2(pos.x, pos.y * aspectRatio), 0.0, 1.0);		
}	