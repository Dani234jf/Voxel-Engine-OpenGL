#pragma once
#include "../Engine/engine.h"
#include "../Physics/physics.h"
#include "../World/world.h"
#include "../Render/render.h"
#include "game.h"


static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
	glViewport(0, 0, width, height);
}

void Game::init(GLFWwindow *window) {
	Renderer::setup();
	Chunk::initNoiseParams();
	glfwSetWindowSizeCallback(window, framebuffer_size_callback);
}

void Game::update(float deltaTime, GLFWwindow *window) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	Player::world.loadChunks(Player::playerTransform->position);

	Renderer::renderWorldChunks(Player::playerTransform->position, Player::cameraComponent, Player::world, ChunkMeshType::SOLID);
	Renderer::renderWorldChunks(Player::playerTransform->position, Player::cameraComponent, Player::world, ChunkMeshType::WATER);
	Renderer::renderWorldChunks(Player::playerTransform->position, Player::cameraComponent, Player::world, ChunkMeshType::DECORATION);
	Renderer::renderCross();

	KeyStates::updateKeysState(window);

	glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
}

std::unique_ptr<Object> Player::camera;
std::unique_ptr<Object> Player::player;
Transform *Player::playerTransform;
Camera *Player::cameraComponent;
World Player::world;

float Player::sprintTimer = 0.0f;
bool Player::sprintTimeout = false;
bool Player::sprinting = false;
const float Player::playerHeight = 1.8f;

void Player::init(GLFWwindow *window) {
	
	Texture atlasTexture = Texture("../../../Assets/blockTexturesAtlas.png");
	world.blockAtlas = { atlasTexture };
	atlasTexture.setActive();

	camera = std::make_unique<Object>();
	camera->addComponent<Transform>();
	camera->addComponent<Camera>(camera->getComponent<Transform>());
	cameraComponent = camera->getComponent<Camera>();
	
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowUserPointer(window, camera->getComponent<Camera>());
	glfwSetCursorPosCallback(window, Camera::cameraPosCallBack);

	player = std::make_unique<Object>();
	player->addComponent<Transform>();
	playerTransform = player->getComponent<Transform>();
	playerTransform->position = glm::vec3(-3.0f, 150.0f, 0);
	player->addComponent<RigidBody>(playerTransform);
	
	player->addComponent<BoxCollider>(playerTransform, glm::vec3(Chunk::blockSize * 0.6f, Chunk::blockSize * playerHeight, Chunk::blockSize * 0.6f));

	glm::ivec2 chunkPos = Chunk::convertToChunkPos(playerTransform->position);
	glm::ivec2 blockPos = glm::ivec2(floor(playerTransform->position.x / Chunk::blockSize), floor(playerTransform->position.z / Chunk::blockSize));

	world.loadChunks(playerTransform->position);
}

static bool playerSetYPos = true;
static glm::ivec2 windowSize;

void Player::update(float deltaTime, GLFWwindow *window) {

	glm::ivec2 lastWindowSize = windowSize;
	glfwGetWindowSize(window, &windowSize.x, &windowSize.y);

	if ( lastWindowSize != windowSize && windowSize.y != 0 ) {
		cameraComponent->aspectRatio = static_cast<float>(windowSize.x) / windowSize.y;
	}

	glm::ivec2 chunkPos = Chunk::convertToChunkPos(playerTransform->position);
	glm::ivec2 blockPos = glm::ivec2(floor(playerTransform->position.x / Chunk::blockSize), floor(playerTransform->position.z / Chunk::blockSize));	

	if ( playerSetYPos ) {
		Chunk *chunk = world.getChunk(chunkPos);
	
		if ( chunk ) {
			glm::ivec2 localBlockPos = blockPos % Chunk::size;

			if ( localBlockPos.x < 0 ) { localBlockPos.x += Chunk::size; }
			if ( localBlockPos.y < 0 ) { localBlockPos.y += Chunk::size; }

			for ( int i = Chunk::height - 1; i >= 0; i-- ) {
				BLOCK_TYPE *blockType = gridAt(chunk->grid, Chunk::size, localBlockPos.x, localBlockPos.y, i);
				if ( blockType != nullptr && *blockType != BLOCK_TYPE::AIR ) {
					playerTransform->position.y = ( i + 1 ) * Chunk::blockSize;
					break;
				}
			}
			playerSetYPos = false;
		}
	}

	glm::vec3 frontN = cameraComponent->front;
	frontN.y = 0.0f;
	frontN = glm::normalize(frontN);

	glm::vec3 rightN = cameraComponent->right;
	rightN.y = 0.0f;
	rightN = glm::normalize(rightN);

	RigidBody *playerRigidBody = player->getComponent<RigidBody>();
	glm::vec3 mov = glm::vec3(0.0f);


	float maxTime = 0.2f;

	if ( sprinting && !isPressed(GLFW_KEY_W, KeyType::KEYBOARD) ) {
		sprinting = false;
	}

	if ( sprintTimeout ) {
		sprintTimer += deltaTime;
		if ( sprintTimer > maxTime ) {
			sprintTimer = 0.0f;
			sprintTimeout = false;
		}

		if ( isClicked(GLFW_KEY_W, KeyType::KEYBOARD) ) {
			sprinting = true;
			sprintTimeout = false;
			sprintTimer = 0.0f;
		}
	}

	if ( isClicked(GLFW_KEY_W, KeyType::KEYBOARD) && !sprintTimeout && !sprinting ) {
		sprintTimeout = true;
		sprintTimer = 0.0f;
	}

	float insideWaterPlayer = 1.0f - glm::clamp((playerHeight * 0.5f + playerTransform->position.y - Chunk::height * Chunk::blockSize * 0.513f) / playerHeight, 0.0f, 1.0f);

	if ( isPressed(GLFW_KEY_W, KeyType::KEYBOARD) ) {
		mov += frontN;
	}
	if ( isPressed(GLFW_KEY_S, KeyType::KEYBOARD) ) {
		mov -= frontN;
	}
	if ( isPressed(GLFW_KEY_D, KeyType::KEYBOARD) ) {
		mov += rightN;
	}
	if ( isPressed(GLFW_KEY_A, KeyType::KEYBOARD) ) {
		mov -= rightN;
	}
	if ( isPressed(GLFW_KEY_SPACE, KeyType::KEYBOARD) && insideWaterPlayer > 0 ) {		
		playerRigidBody->velocity.y = glm::mix(4.5f, 1.5f, insideWaterPlayer);
	}
	else if ( isPressed(GLFW_KEY_SPACE, KeyType::KEYBOARD) && playerRigidBody->onGround ) {
		playerRigidBody->velocity.y = 4.5f;
	}

	if ( glm::length(mov) > 0 ) {
		mov = glm::normalize(mov);
	}

	mov *= 4.0f * ( sprinting ? 1.4f : 1.0f );

	playerRigidBody->velocity.x = mov.x;
	playerRigidBody->velocity.z = mov.z;

	if ( !playerRigidBody->onGround ) {
		playerRigidBody->velocity *= glm::mix(1.0f, 0.9f, insideWaterPlayer);
	}


	player->getComponent<RigidBody>()->update(deltaTime);

	BoxCollider *playerBoxColider = player->getComponent<BoxCollider>();
	Colision::updateRigidBody(playerBoxColider, world, playerRigidBody, deltaTime);

	camera->getComponent<Transform>()->position = playerTransform->position + glm::vec3(0.0f, 0.4f, 0.0f);

	Ray ray = { camera->getComponent<Transform>()->position, glm::normalize(cameraComponent->front), Chunk::blockSize * 0.01f, Chunk::blockSize * Chunk::size * 0.4f };
	RayInfo rayInfo = RayCaster::castRay(world, ray);

	if ( rayInfo.hit ) {

		Chunk *chunk = world.getChunk(rayInfo.chunkPos);
		std::vector<glm::ivec2> updateChunksMeshPos;
		BLOCK_TYPE blockTypeHit = chunk->grid[rayInfo.blockIndex];

		glm::ivec3 blockPos = rayInfo.blockPosLocal;
		glm::ivec2 chunkPos = rayInfo.chunkPos;
		bool blockUpdated = false;

		if ( isClicked(GLFW_MOUSE_BUTTON_LEFT, KeyType::MOUSE) ) {
			chunk->grid[rayInfo.blockIndex] = BLOCK_TYPE::AIR;
			updateChunksMeshPos.push_back(rayInfo.chunkPos);
			blockUpdated = true;
		}
		else if ( isClicked(GLFW_MOUSE_BUTTON_RIGHT, KeyType::MOUSE) ) {
			blockPos += rayInfo.faceNormal;
			bool insideChunk = blockPos.x >= 0 && blockPos.x < Chunk::size && blockPos.y >= 0 && blockPos.y < Chunk::size;

			if ( rayInfo.faceNormal.z != 0 || insideChunk ) {

				glm::vec3 worldBlockPos = glm::vec3(blockPos.x + rayInfo.chunkPos.x * Chunk::size, blockPos.z, blockPos.y + rayInfo.chunkPos.y * Chunk::size);
				worldBlockPos *= Chunk::blockSize;
				AABB blockAABB = { worldBlockPos, worldBlockPos + glm::vec3(Chunk::blockSize) };
				AABB playerAABB = playerBoxColider->getAABB();

				if ( !Colision::AABBIntersect(blockAABB, playerAABB) ) {
					chunk->grid[blockPos.x + ( blockPos.y + blockPos.z * Chunk::size ) * Chunk::size] = BLOCK_TYPE::DIRT;
					updateChunksMeshPos.push_back(chunkPos);
					blockUpdated = true;
				}
			}
			else if ( rayInfo.faceNormal.z == 0 ) {
				glm::ivec3 globalBlockPos = rayInfo.blockPosLocal + rayInfo.faceNormal + glm::ivec3(rayInfo.chunkPos.x * Chunk::size, rayInfo.chunkPos.y * Chunk::size, 0);

				blockPos = glm::ivec3(blockPos.x % Chunk::size, blockPos.y % Chunk::size, blockPos.z);
				if ( blockPos.x < 0 ) { blockPos.x = blockPos.x + Chunk::size; }
				if ( blockPos.y < 0 ) { blockPos.y = blockPos.y + Chunk::size; }

				chunkPos = glm::ivec2(floor((float) globalBlockPos.x / Chunk::size), floor((float) globalBlockPos.y / Chunk::size));
				Chunk *otherChunk = world.getChunk(chunkPos);

				glm::vec3 worldBlockPos = glm::vec3(globalBlockPos.x * Chunk::blockSize, globalBlockPos.z * Chunk::blockSize, globalBlockPos.y * Chunk::blockSize);
				AABB blockAABB = { worldBlockPos, worldBlockPos + glm::vec3(Chunk::blockSize) };
				AABB playerAABB = playerBoxColider->getAABB();

				if ( otherChunk && !Colision::AABBIntersect(blockAABB, playerAABB) ) {
					otherChunk->grid[blockPos.x + ( blockPos.y + blockPos.z * Chunk::size ) * Chunk::size] = BLOCK_TYPE::DIRT;
					updateChunksMeshPos.push_back(chunkPos);
					blockUpdated = true;
				}
			}
		}

		if ( blockUpdated ) {
			if ( blockPos.x == 0 ) {
				updateChunksMeshPos.push_back(chunkPos - glm::ivec2(1, 0));
			}
			if ( blockPos.x == Chunk::size - 1 ) {
				updateChunksMeshPos.push_back(chunkPos + glm::ivec2(1, 0));
			}
			if ( blockPos.y == 0 ) {
				updateChunksMeshPos.push_back(chunkPos - glm::ivec2(0, 1));
			}
			if ( blockPos.y == Chunk::size - 1 ) {
				updateChunksMeshPos.push_back(chunkPos + glm::ivec2(0, 1));
			}
		}

		// draw wireframe
		glm::vec3 worldPos = glm::vec3(blockPos.x, blockPos.z, blockPos.y) + glm::vec3(chunkPos.x * Chunk::size, 0, chunkPos.y * Chunk::size);
		worldPos *= Chunk::blockSize;
		
		Renderer::renderWireFrame(worldPos, cameraComponent);		

		for ( glm::ivec2 &pos : updateChunksMeshPos ) {		
			{
				std::lock_guard<std::mutex> lock(world.generationMutex);
				world.generationDeque.push_front({pos, ChunkJobType::UpdateMesh});
			}
		}
	}
}
