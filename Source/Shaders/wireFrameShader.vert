#version 330 core
layout (location = 0) in vec3 pos;

uniform mat4 model;
uniform mat4 viewProject;	
		
void main() {
	gl_Position = viewProject * model * vec4(pos, 1.0);		
}	