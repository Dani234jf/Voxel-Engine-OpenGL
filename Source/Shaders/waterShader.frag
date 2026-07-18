#version 330 core
		
uniform sampler2D atlas;
uniform vec3 cameraPos;
in vec3 normal;
in vec2 texCoord;
in vec3 worldPos;
out vec4 fragColor;

void main() {
	vec4 texColor = texture(atlas, texCoord);

	vec3 lightDir = normalize(vec3(0.2f, 2.0f, 0.2f));
	float brightness = max(0.5, dot(lightDir, normal)) * 1.25;

	// specular highlight
	vec3 viewDir = normalize(cameraPos - worldPos);
	vec3 H = normalize(lightDir + viewDir);
	float spec = pow(max(dot(normal, H), 0.0), 16.0);

	vec4 finalColor = vec4((texColor * brightness).rgb + spec * 0.2 * vec3(0.9, 1.0, 0.7), 0.8);

	float dist = length(cameraPos - worldPos);
	float fog = smoothstep(90.0, 100.0, dist);

	fragColor = mix(finalColor, vec4(0.53, 0.81, 0.92, 0.8), fog);
}