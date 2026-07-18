#include "Engine/engine.h"
#include "Game/game.h"
#include "Render/render.h"
#include "World/world.h"
#include "Physics/physics.h"

int main() {
		
	auto *window = windowInit();

	if ( !window ) {
		return -1;
	}	

	Game::init(window);
	Player::init(window);
	
	float lastTime = 0.0f;

	glfwSwapInterval(0);

	while ( !glfwWindowShouldClose(window) ) {

		float currentTime = glfwGetTime();
		float deltaTime = std::min(currentTime - lastTime, 1.0f / 60.0f);
		lastTime = currentTime;
		
		if ( glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ) {
			glfwSetWindowShouldClose(window, true);
		}		

		Game::update(deltaTime, window);
		Player::update(deltaTime, window);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	windowExit(window);
	
	return 0;
}