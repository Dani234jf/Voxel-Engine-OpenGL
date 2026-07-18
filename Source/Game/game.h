#pragma once
#include "../World/world.h"
#include <mutex>
#include <thread>

static class Game {
public:
	static void init(GLFWwindow *window);
	static void update(float deltaTime, GLFWwindow *window);
};


static class Player {
private:
	static std::unique_ptr<Object> camera;
	static std::unique_ptr<Object> player;

	static float sprintTimer;
	static bool sprintTimeout;
	static bool sprinting;
	static const float playerHeight;
public:
	static World world;
	static Camera *cameraComponent;
	static Transform *playerTransform;

	static void init(GLFWwindow *window);
	static void update(float deltaTime, GLFWwindow *window);
};
