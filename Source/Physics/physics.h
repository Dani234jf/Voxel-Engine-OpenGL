#pragma once
#include <glm/glm.hpp>
#include "../World/world.h"

struct AABB {
	glm::vec3 min;
	glm::vec3 max;
};

class BoxCollider : public Component {
private:
	Transform *transform;
public:
	glm::vec3 halfSize;

	BoxCollider(Transform *transform, glm::vec3 dimensions);
	AABB getAABB();
};

class RigidBody : public Component {
public:
	Transform *transform;
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::vec3 accelaration = glm::vec3(0.0f);
	bool onGround = false;

	RigidBody(Transform *transform);
	void update(float deltaTime);
	void applyForce(glm::vec3 force);
};


typedef struct {
	glm::vec3 rayOrigin;
	glm::vec3 rayDirection;
	float step;
	float maxDist;
} Ray;

typedef struct {
	bool hit;
	glm::ivec2 chunkPos;
	glm::ivec3 blockPosLocal;
	glm::vec3 position;
	glm::ivec3 faceNormal;
	int blockIndex;
} RayInfo;

static class RayCaster {
public:
	static RayInfo castRay(World &world, Ray &ray);
};

static class Colision {
public:
	static bool AABBIntersect(AABB &a, AABB &b);
	static void updateRigidBody(BoxCollider *boxColider, World &world, RigidBody *rigidBody, float deltaTime);
	static bool detectColisions(BoxCollider *boxColider, World &world);
};