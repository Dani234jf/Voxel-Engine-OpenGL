#version 330 core
		
uniform sampler2D atlas;
uniform vec3 cameraPos;
in vec3 normal;
in vec2 texCoord;
in vec3 worldPos;
in float vertAO;
out vec4 fragColor;

void main() {

	vec4 texColor = texture(atlas, texCoord);
	if ( texColor.a < 0.5 ) {
		discard;
	}

	vec3 lightDir = normalize(vec3(0.2f, 2.0f, 0.2f));
	float brightness = max(0.5, dot(lightDir, normal));
	
	float dist = length(cameraPos - worldPos);
	float fog = smoothstep(90.0, 100.0, dist);

	fragColor = mix(texColor * brightness * vertAO, vec4(0.53, 0.81, 0.92, 1), fog);
}