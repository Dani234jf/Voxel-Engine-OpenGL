#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normalDir;
layout (location = 2) in vec2 uv;
		
out vec3 normal;
out vec2 texCoord;
out vec3 worldPos;

uniform mat4 model;
uniform mat4 viewProject;	
		
void main() {
	worldPos = (model * vec4(pos, 1.0)).xyz;
	normal = normalize( mat3(transpose(inverse(model))) * normalize(normalDir) );	
	texCoord = uv;
	gl_Position = viewProject * model * vec4(pos - vec3(0, 0.1, 0), 1.0);		
}