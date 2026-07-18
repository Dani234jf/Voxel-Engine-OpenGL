#pragma once
#include <glm/glm.hpp>
#include "../World/world.h"

class World;
enum class ChunkMeshType;

static class Renderer {
public:
	static std::unique_ptr<Shader> blocksShader;
	static std::unique_ptr<Shader> waterShader;
	static std::unique_ptr<Shader> wireFrameShader;
	static std::unique_ptr<Shader> uiShader;
	static GLuint cubeVAO;
	static GLuint crossVAO;

	static void setup();
	static void renderWireFrame(glm::vec3 worldPos, Camera *cameraComponent);
	static void renderCross();
	static void renderWorldChunks(glm::vec3 worldPos, Camera *camera, World &world, ChunkMeshType chunkMeshType);
};

