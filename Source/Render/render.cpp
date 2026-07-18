#include "../Engine/engine.h"
#include "render.h"

std::unique_ptr<Shader> Renderer::blocksShader = nullptr;
std::unique_ptr<Shader> Renderer::waterShader = nullptr;
std::unique_ptr<Shader> Renderer::wireFrameShader = nullptr;
std::unique_ptr<Shader> Renderer::uiShader = nullptr;
GLuint Renderer::cubeVAO;
GLuint Renderer::crossVAO;

void Renderer::renderWorldChunks(glm::vec3 worldPos, Camera *camera, World &world, ChunkMeshType chunkMeshType) {	

	Shader *shader = nullptr;

	switch ( chunkMeshType ) {
		case ChunkMeshType::WATER:
			shader = waterShader.get();
			break;
		case ChunkMeshType::SOLID:
		case ChunkMeshType::DECORATION:
			shader = blocksShader.get();
			break;		
	}

	shader->use();

	unsigned int modelLoc = glGetUniformLocation(shader->shaderProgram, "model");
	unsigned int viewProjectLoc = glGetUniformLocation(shader->shaderProgram, "viewProject");
	unsigned int atlasLoc = glGetUniformLocation(shader->shaderProgram, "atlas");

	glUniformMatrix4fv(viewProjectLoc, 1, GL_FALSE, glm::value_ptr(camera->getViewProjectMatrix()));
	glUniform3fv(glGetUniformLocation(shader->shaderProgram, "cameraPos"), 1, glm::value_ptr(camera->cameraTransform->position));
	glUniform1i(atlasLoc, 0);

	glm::ivec2 chunkPos = glm::ivec2(floor(worldPos.x / ( Chunk::blockSize * Chunk::size )), floor(worldPos.z / ( Chunk::blockSize * Chunk::size )));
	float radiusSquared = world.chunkGenerationRadius * world.chunkGenerationRadius;

	for ( int i = -world.chunkGenerationRadius; i <= world.chunkGenerationRadius; i++ ) {
		for ( int j = -world.chunkGenerationRadius; j <= world.chunkGenerationRadius; j++ ) {
			if ( i * i + j * j > radiusSquared ) {
				continue;
			}

			glm::vec2 chunkPosWorld = (glm::vec2)( chunkPos + glm::ivec2(i, j) ) * ( Chunk::size * Chunk::blockSize );
			glm::vec2 toChunk = chunkPosWorld - glm::vec2(camera->cameraTransform->position.x, camera->cameraTransform->position.z);

			if ( glm::length(toChunk) > Chunk::blockSize * Chunk::size * 2 && glm::dot(glm::normalize(glm::vec2(camera->front.x, camera->front.z)), glm::normalize(toChunk)) < 0.25f ) {
				continue;
			}			

			auto *chunkMeshObj = world.getChunkObj(chunkPos + glm::ivec2(i, j));

			if ( chunkMeshObj ) {
				
				if ( ( chunkMeshType == ChunkMeshType::SOLID && chunkMeshObj->solidMesh->getComponent<Mesh>()->isEmpty() ) || ( chunkMeshType == ChunkMeshType::WATER && chunkMeshObj->waterMesh->getComponent<Mesh>()->isEmpty() ) || ( chunkMeshType == ChunkMeshType::DECORATION && chunkMeshObj->decorationMesh->getComponent<Mesh>()->isEmpty() ) ) {
					continue;
				}

				glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(chunkMeshObj->solidMesh->getComponent<Transform>()->getModelMatrix()));
				
				switch ( chunkMeshType ) {
				case ChunkMeshType::WATER:
					glEnable(GL_BLEND);
					glDepthMask(GL_FALSE);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					glActiveTexture(GL_TEXTURE0);					
					chunkMeshObj->waterMesh->getComponent<Mesh>()->draw();
					glDepthMask(GL_TRUE);
					glDisable(GL_BLEND);
					break;
				case ChunkMeshType::SOLID:
					chunkMeshObj->solidMesh->getComponent<Mesh>()->draw();
					break;
				case ChunkMeshType::DECORATION:
					glDisable(GL_CULL_FACE);
					chunkMeshObj->decorationMesh->getComponent<Mesh>()->draw();
					glEnable(GL_CULL_FACE);
					break;
				}
			}
		}
	}
}

void Renderer::renderWireFrame(glm::vec3 worldPos, Camera *cameraComponent) {

	wireFrameShader->use();
	glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos);

	glUniformMatrix4fv(glGetUniformLocation(wireFrameShader->shaderProgram, "viewProject"), 1, GL_FALSE, glm::value_ptr(cameraComponent->getViewProjectMatrix()));
	glUniformMatrix4fv(glGetUniformLocation(wireFrameShader->shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

	glLineWidth(1.6f);

	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_LINES, 0, 24);
	glBindVertexArray(0);

}

void Renderer::setup() {
	blocksShader = std::make_unique<Shader>("../../../Source/Shaders/blockShader.vert", "../../../Source/Shaders/blockShader.frag");
	waterShader = std::make_unique<Shader>("../../../Source/Shaders/waterShader.vert", "../../../Source/Shaders/waterShader.frag");
	wireFrameShader = std::make_unique<Shader>("../../../Source/Shaders/wireFrameShader.vert", "../../../Source/Shaders/wireFrameShader.frag");
	uiShader = std::make_unique<Shader>("../../../Source/Shaders/uiShader.vert", "../../../Source/Shaders/uiShader.frag");
	
	// wireFrame cube

	glm::vec3 verticesCube[] =
	{
		{0,0,0},{1,0,0},
		{1,0,0},{1,1,0},
		{1,1,0},{0,1,0},
		{0,1,0},{0,0,0},

		{0,0,1},{1,0,1},
		{1,0,1},{1,1,1},
		{1,1,1},{0,1,1},
		{0,1,1},{0,0,1},

		{0,0,0},{0,0,1},
		{1,0,0},{1,0,1},
		{1,1,0},{1,1,1},
		{0,1,0},{0,1,1},
	};

	for ( int i = 0; i < 24; i++ ) {
		verticesCube[i] *= Chunk::blockSize * 1.001f;
	}

	GLuint cubeVBO;

	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);

	glBindVertexArray(cubeVAO);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verticesCube), verticesCube, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *) 0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);

	// cross

	float verticesCross[] = {
		-0.012f,  0.0f,
		 0.012f,  0.0f,

		 0.0f, -0.012f,
		 0.0f,  0.012f
	};

	GLuint VBO;	

	glGenVertexArrays(1, &crossVAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(crossVAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(verticesCross), verticesCross, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);

}

void Renderer::renderCross() {	
	uiShader->use();

	glBindVertexArray(crossVAO);

	glDisable(GL_DEPTH_TEST);
	glLineWidth(2.5f);
	glDrawArrays(GL_LINES, 0, 4);
	glEnable(GL_DEPTH_TEST);
}
