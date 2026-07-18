#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <random>
#include <cstdint>
#include <unordered_set>
#include <fstream>
#include <sstream>


struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
	float ao;
};


class Component {
public:
	virtual ~Component() = default;
};

class Transform : public Component {
public:
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	bool modelMatrixGen = false;
	bool isStatic = false;

	glm::mat4 getModelMatrix();
};

class Camera : public Component {
private:
	float yaw = 0.0f;
	float pitch = 0.0f;
public:
	Transform *cameraTransform;
	glm::mat4 projection = glm::mat4(1.0f);
	glm::mat4 view = glm::mat4(1.0f);
	float sensitivity = 0.1f;
	float fovAngle = 60.0f;
	float aspectRatio = 1280.0f / 720.0f;
	glm::vec3 front = glm::vec3(0.0f);
	glm::vec3 right = glm::vec3(0.0f);

	Camera(Transform *cameraTransform);

	glm::mat4 getViewProjectMatrix();

	static void cameraPosCallBack(GLFWwindow *window, double xpos, double ypos);
	void processMouse(double xpos, double ypos);
};

class Object {
private:
	std::vector<std::unique_ptr<Component>> components;
public:

	template <typename T, typename... Args>
	void addComponent(Args&&... args) {
		T *component = new T(std::forward<Args>(args)...);
		components.push_back(std::unique_ptr<Component>(component));
	}

	template <typename T>
	T *getComponent() {
		for ( auto &c : components ) {
			auto casted = dynamic_cast<T *>( c.get() );
			if ( casted != nullptr ) {
				return casted;
			}
		}
		return nullptr;
	}
};

class Mesh : public Component {
private:
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	unsigned int VBO, VAO, EBO;
public:
	Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices);
	void updateMesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices);
	void draw();
	bool isEmpty();
};

class Texture {
private:
	unsigned char *data;
	int channels;
	GLuint textureID;
public:
	int width;
	int height;
	Texture() {
		data = nullptr;
	}
	Texture(const char *texturePath);
	void setActive();
};

std::string readFile(const std::string &filePath);

class Shader {
private:
	unsigned int vertexShader;
	unsigned int fragmentShader;
public:
	unsigned int shaderProgram;

	Shader(const std::string &vertexShaderPath, const std::string &fragmentShaderPath);
	~Shader();
	void use();
};


struct KeyState {
	unsigned int keyType;
	bool isPressed;
	bool isClicked;
	bool following;
};

enum KeyType {
	MOUSE,
	KEYBOARD
};

static class KeyStates {
private:
	static const int KEY_BUFFER_SIZE = GLFW_KEY_LAST + 1;
	inline static KeyState keyStates[KEY_BUFFER_SIZE];
public:
	static KeyState &getKeyState(unsigned int keyCode, unsigned int type);
	static void updateKeysState(GLFWwindow *window);
};

static bool isPressed(unsigned int keyCode, unsigned int type) {
	return KeyStates::getKeyState(keyCode, type).isPressed;
}
static bool isClicked(unsigned int keyCode, unsigned int type) {
	return KeyStates::getKeyState(keyCode, type).isClicked;
}


GLFWwindow *windowInit();

void windowExit(GLFWwindow *window);