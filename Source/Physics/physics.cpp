#include "../Engine/engine.h"
#include "../World/world.h"
#include "physics.h"

BoxCollider::BoxCollider(Transform *transform, glm::vec3 dimensions) {
	this->transform = transform;
	this->halfSize = dimensions / 2.0f;
}

AABB BoxCollider::getAABB() {
	return { transform->position - halfSize, transform->position + halfSize };
}


RigidBody::RigidBody(Transform *transform) {
	this->transform = transform;
}

void RigidBody::update(float deltaTime) {
	velocity.y += -15.0f * deltaTime;

	velocity += accelaration * deltaTime;
	accelaration = { 0.0f, 0.0f, 0.0f };
	onGround = false;
}

void RigidBody::applyForce(glm::vec3 force) {
	accelaration += force;
}


RayInfo RayCaster::castRay(World &world, Ray &ray) {
	RayInfo rayInfo = { 0 };

	glm::ivec3 previusBlockPos = glm::ivec3(floor(ray.rayOrigin.x / Chunk::blockSize), floor(ray.rayOrigin.y / Chunk::blockSize), floor(ray.rayOrigin.z / Chunk::blockSize));

	for ( float t = 0.0f; t < ray.maxDist; t += ray.step ) {

		glm::vec3 pos = ray.rayOrigin + ray.rayDirection * t;

		glm::ivec2 blockPos = glm::ivec2(floor(pos.x / Chunk::blockSize), floor(pos.z / Chunk::blockSize));
		glm::ivec2 chunkPos = glm::ivec2(floor(pos.x / ( Chunk::size * Chunk::blockSize )), floor(pos.z / ( Chunk::size * Chunk::blockSize )));
		glm::ivec3 blockPosLocal = glm::ivec3(blockPos.x % Chunk::size, blockPos.y % Chunk::size, floor(pos.y / Chunk::blockSize));

		blockPosLocal.x = blockPosLocal.x < 0 ? blockPosLocal.x + Chunk::size : blockPosLocal.x;
		blockPosLocal.y = blockPosLocal.y < 0 ? blockPosLocal.y + Chunk::size : blockPosLocal.y;

		Chunk *chunk = world.getChunk(chunkPos);

		if ( !chunk ) {
			continue;
		}

		int index = blockPosLocal.x + blockPosLocal.y * Chunk::size + blockPosLocal.z * Chunk::size * Chunk::size;

		if ( index >= 0 && index < ( Chunk::size * Chunk::size * Chunk::height ) && ( chunk->grid[index] != BLOCK_TYPE::AIR && chunk->grid[index] != BLOCK_TYPE::WATER ) ) {
			rayInfo.hit = true;
			rayInfo.position = pos;
			rayInfo.chunkPos = chunkPos;
			rayInfo.blockPosLocal = blockPosLocal;
			rayInfo.blockIndex = index;
			rayInfo.faceNormal = previusBlockPos - glm::ivec3(blockPos.x, blockPos.y, floor(pos.y / Chunk::blockSize));
			return rayInfo;
		}
		previusBlockPos = glm::ivec3(blockPos.x, blockPos.y, floor(pos.y / Chunk::blockSize));
	}
	return rayInfo;
}


bool Colision::AABBIntersect(AABB &a, AABB &b) {
	return ( a.min.x <= b.max.x && a.max.x >= b.min.x ) &&
		( a.min.y <= b.max.y && a.max.y >= b.min.y ) &&
		( a.min.z <= b.max.z && a.max.z >= b.min.z );
}

void Colision::updateRigidBody(BoxCollider *boxColider, World &world, RigidBody *rigidBody, float deltaTime) {

	rigidBody->transform->position.x += rigidBody->velocity.x * deltaTime;
	if ( detectColisions(boxColider, world) ) {
		rigidBody->transform->position.x -= rigidBody->velocity.x * deltaTime;
		rigidBody->velocity.x = 0;
	}

	rigidBody->transform->position.y += rigidBody->velocity.y * deltaTime;
	if ( detectColisions(boxColider, world) ) {
		rigidBody->onGround = rigidBody->velocity.y < 0;

		rigidBody->transform->position.y -= rigidBody->velocity.y * deltaTime;
		rigidBody->velocity.y = 0;

	}

	rigidBody->transform->position.z += rigidBody->velocity.z * deltaTime;
	if ( detectColisions(boxColider, world) ) {
		rigidBody->transform->position.z -= rigidBody->velocity.z * deltaTime;
		rigidBody->velocity.z = 0;
	}

}

bool Colision::detectColisions(BoxCollider *boxColider, World &world) {
	AABB aabb = boxColider->getAABB();

	// convert to globalBlockCoords
	glm::ivec3 globalBlockPosMin = glm::floor(aabb.min / Chunk::blockSize);
	glm::ivec3 globalBlockPosMax = glm::floor(aabb.max / Chunk::blockSize);

	int count = 0;
	for ( int blockPosX = globalBlockPosMin.x; blockPosX <= globalBlockPosMax.x; blockPosX++ ) {
		for ( int blockPosY = globalBlockPosMin.y; blockPosY <= globalBlockPosMax.y; blockPosY++ ) {
			for ( int blockPosZ = globalBlockPosMin.z; blockPosZ <= globalBlockPosMax.z; blockPosZ++ ) {

				if ( blockPosY < 0 || blockPosY >= Chunk::height ) {
					continue;
				}

				glm::ivec2 chunkPos = glm::ivec2(floor((float) blockPosX / Chunk::size), floor((float) blockPosZ / Chunk::size));
				glm::ivec3 localBlockPos = glm::ivec3(blockPosX % Chunk::size, blockPosZ % Chunk::size, blockPosY);

				if ( localBlockPos.x < 0 ) localBlockPos.x += Chunk::size;
				if ( localBlockPos.y < 0 ) localBlockPos.y += Chunk::size;

				Chunk *chunk = world.getChunk(chunkPos);
				if ( !chunk ) {
					continue;
				}

				BLOCK_TYPE *blockType = gridAt(chunk->grid, Chunk::size, localBlockPos.x, localBlockPos.y, localBlockPos.z);
				if ( blockType != nullptr && ( *blockType == BLOCK_TYPE::AIR || *blockType == BLOCK_TYPE::WATER || *blockType == BLOCK_TYPE::SHORT_GRASS ) ) {
					continue;
				}

				glm::vec3 blockOrigin = glm::vec3(blockPosX, blockPosY, blockPosZ) * Chunk::blockSize;
				AABB aabbBlock = { blockOrigin, blockOrigin + glm::vec3(Chunk::blockSize) };

				if ( AABBIntersect(aabb, aabbBlock) ) {
					return true;
				}
			}
		}
	}
	return false;
}