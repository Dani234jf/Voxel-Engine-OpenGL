#include "engine.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

glm::mat4 Transform::getModelMatrix() {

	if ( !modelMatrixGen || !isStatic ) {
		glm::mat4 model = glm::mat4(1.0f);

		model = glm::translate(model, position);

		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

		model = glm::scale(model, scale);

		modelMatrix = model;
		modelMatrixGen = true;
		return model;
	}	
	return modelMatrix;			
}

Camera::Camera(Transform *cameraTransform) {
	this->cameraTransform = cameraTransform;
}

glm::mat4 Camera::getViewProjectMatrix() {

	float yawRadians = glm::radians(yaw);
	float pitchRadians = glm::radians(pitch);

	glm::vec3 front;
	glm::vec3 up = glm::vec3(0, 1, 0);

	front.x = cos(yawRadians) * cos(pitchRadians);
	front.y = sin(pitchRadians);
	front.z = sin(yawRadians) * cos(pitchRadians);

	front = glm::normalize(front);

	this->front = front;
	this->right = glm::normalize(glm::cross(front, up));

	projection = glm::perspective(
		glm::radians(fovAngle),
		aspectRatio,
		0.01f,
		1000.0f
	);

	view = glm::lookAt(cameraTransform->position, cameraTransform->position + front, up);

	return projection * view;
}

void Camera::processMouse(double xpos, double ypos) {
	static double lastX = 1280 / 2;
	static double lastY = 720 / 2;
	static bool firstMouse = true;

	if ( firstMouse ) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	double xoffset = xpos - lastX;
	double yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	yaw += xoffset * sensitivity;
	pitch += yoffset * sensitivity;
	pitch = pitch > 89.0f ? 89.0f : pitch < -89.0f ? -89.0f : pitch;
}

void Camera::cameraPosCallBack(GLFWwindow *window, double xpos, double ypos) {
	Camera *camera = (Camera *) glfwGetWindowUserPointer(window);
	if ( !camera ) { printf("[ERROR]: camera = nullptr (Camera::cameraPosCallBack(...))\n"); return; }
	camera->processMouse(xpos, ypos);
}


Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices) {
	this->vertices = vertices;
	this->indices = indices;

	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
	glGenVertexArrays(1, &VAO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, normal));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, uv));
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, ao));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);

}

void Mesh::updateMesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices) {
	this->vertices = vertices;
	this->indices = indices;

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);
}

void Mesh::draw() {
	glBindVertexArray(this->VAO);
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
}

bool Mesh::isEmpty() {
	return vertices.size() == 0;
}

Texture::Texture(const char *texturePath) {
	data = stbi_load(texturePath, &width, &height, &channels, 0);

	if ( !data ) {
		printf("ERROR: the texture was not generated!");
		return;
	}

	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	GLenum format = channels == 1 ? GL_RED : channels == 3 ? GL_RGB : GL_RGBA;

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);
}

void Texture::setActive() {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureID);
}

std::string readFile(const std::string &filePath) {
	std::ifstream file(filePath);

	std::stringstream buffer;

	buffer << file.rdbuf();

	return buffer.str();
}

Shader::Shader(const std::string &vertexShaderPath, const std::string &fragmentShaderPath) {

	std::string vertShaderStr = readFile(vertexShaderPath);
	std::string fragShaderStr = readFile(fragmentShaderPath);

	const char *shaderVert = vertShaderStr.c_str();
	const char *shaderFrag = fragShaderStr.c_str();

	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &shaderVert, NULL);
	glCompileShader(vertexShader);

	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &shaderFrag, NULL);
	glCompileShader(fragmentShader);

	shaderProgram = glCreateProgram();

	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
}

Shader::~Shader() {
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::use() {
	glUseProgram(shaderProgram);
}


KeyState &KeyStates::getKeyState(unsigned int keyCode, unsigned int type) {
	keyStates[keyCode].keyType = type;
	keyStates[keyCode].following = true;
	return keyStates[keyCode];
}

void KeyStates::updateKeysState(GLFWwindow *window) {
	for ( int i = 0; i < KEY_BUFFER_SIZE; i++ ) {
		if ( keyStates[i].following ) {
			KeyState *keyState = &keyStates[i];
			bool pressed = keyState->keyType == KeyType::MOUSE ? glfwGetMouseButton(window, i) == GLFW_PRESS : glfwGetKey(window, i) == GLFW_PRESS;
			if ( pressed ) {
				keyState->isClicked = false;
				if ( !keyState->isPressed ) {
					keyState->isClicked = true;
				}
				keyState->isPressed = true;
			}
			else {
				keyState->isPressed = false;
				keyState->isClicked = false;
			}
		}
	}
}

GLFWwindow *windowInit() {

	if ( !glfwInit() ) {
		printf("[ERROR]: GLFW init failed!\n");
		return nullptr;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow *window = glfwCreateWindow(1280, 720, "OpenGl First Project", NULL, NULL);

	if ( !window ) {
		printf("[ERROR]: Window not created!\n");
		glfwTerminate();
		return nullptr;
	}

	glfwMakeContextCurrent(window);

	if ( !gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) ) {
		printf("[ERROR]: Glad init failed!\n");
	}

	glViewport(0, 0, 1280, 720);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	return window;
}

void windowExit(GLFWwindow *window) {
	glfwDestroyWindow(window);
	glfwTerminate();
}