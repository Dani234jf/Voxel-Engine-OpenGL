#pragma once
#include "cstdint"
#include <memory.h>
#include <glm/glm.hpp>
#include <FastNoiseLite.h>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>

enum class BLOCK_TYPE : uint8_t {
	GRASS,
	DIRT,
	STONE,
	SAND,
	WOOD,
	SHORT_GRASS,
	WATER,
	LEAVES,
	AIR,
	NONE
};

class World;

enum BLOCK_DIR {
	UP,
	DOWN,
	DEFAULT
};

class BlockAtlas {
public:
	Texture texture;
	int tileSize = 16;
	BlockAtlas() = default;
	BlockAtlas(Texture texture);
	glm::vec2 getUVcoord(glm::ivec2 texturePos);
	float getTileSize();
};

uint32_t hash(int x, int z, int seed);
float randD(int x, int z);

static inline BLOCK_TYPE *gridAt(BLOCK_TYPE grid[], int size, int i, int j, int k);

struct AODir {
	glm::i8vec3 side0;
	glm::i8vec3 side1;
	glm::i8vec3 corner;
};

class Chunk {
public:
	inline static const float blockSize = 0.5f;
	inline static const int size = 32;
	inline static const int height = 256;
	BLOCK_TYPE grid[size * size * height];
	World *world = nullptr;
	inline static constexpr AODir aoMap[6][4] = {
		{
			{{-1, -1, 0}, {-1, 0, -1}, {-1, -1, -1}}, {{-1, -1, 0}, {-1, 0, 1}, {-1, -1, 1}}, {{-1, 1, 0}, {-1, 0, 1}, {-1, 1, 1}}, {{-1, 1, 0}, {-1, 0, -1}, {-1, 1, -1}}
		},
		{
			{{1, -1, 0}, {1, 0, -1}, {1, -1, -1}}, {{1, -1, 0}, {1, 0, 1}, {1, -1, 1}}, {{1, 1, 0}, {1, 0, 1}, {1, 1, 1}}, {{1, 1, 0}, {1, 0, -1}, {1, 1, -1}}
		},
		{
			{{-1, 0, -1}, {0, -1, -1}, {-1, -1, -1}}, {{-1, 0, -1}, {0, 1, -1}, {-1, 1, -1}}, {{0, 1, -1}, {1, 0, -1}, {1, 1, -1}}, {{0, -1, -1}, {1, 0, -1}, {1, -1, -1}}
		},
		{
			{{-1, 0, 1}, {0, -1, 1}, {-1, -1, 1}}, {{-1, 0, 1}, {0, 1, 1}, {-1, 1, 1}}, {{0, 1, 1}, {1, 0, 1}, {1, 1, 1}}, {{0, -1, 1}, {1, 0, 1}, {1, -1, 1}}
		},
		{
			{{-1, -1, 0}, {0, -1, -1}, {-1, -1, -1}}, {{0, -1, -1}, {1, -1, 0}, {1, -1, -1}}, {{0, -1, 1}, {1, -1, 0}, {1, -1, 1}}, {{-1, -1, 0}, {0, -1, 1}, {-1, -1, 1}}
		},
		{
			{{-1, 1, 0}, {0, 1, -1}, {-1, 1, -1}}, {{0, 1, -1}, {1, 1, 0}, {1, 1, -1}}, {{0, 1, 1}, {1, 1, 0}, {1, 1, 1}}, {{-1, 1, 0}, {0, 1, 1}, {-1, 1, 1}}
		}
	};

	static FastNoiseLite plainsNoise;	
	static FastNoiseLite montainsNoise;
	static FastNoiseLite montainsMask;
	static FastNoiseLite riverNoise;
	static FastNoiseLite oceanNoise;
	static FastNoiseLite continentNoise;

	static glm::ivec2 convertToChunkPos(glm::vec3 worldPos);
	void generateChunk(glm::vec3 worldPos);
	static void initNoiseParams();
	static inline int generateHeight(float x, float y, float seaLevel);
	void getChunkBlocks(glm::ivec2 chunkPos);
	static BLOCK_TYPE getBlock(glm::ivec3 blockPos);
	static float vertexAO(bool side0, bool side1, bool corner);
	void gridSet(glm::ivec2 chunkPos, int i, int j, int k, BLOCK_TYPE block);
	void spawnGrass(glm::ivec2 chunkPos);
	void spawnTrees(glm::ivec2 chunkPos);
	bool isPosOpaque(int i, int j, int k, glm::ivec2 chunkPos, bool isWater);
	bool isSolidAO(int i, int j, int k, glm::ivec2 chunkPos);
	void addFace(int dir, glm::vec3 pos, std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, glm::vec2 baseUV, float tileSize, glm::ivec2 chunkPos);
	void updateWater();
	void generateBlockMesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, glm::ivec2 chunkPos, BlockAtlas &blockAtlas, bool isWater);
	void generateDecorationMesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, BlockAtlas &blockAtlas);
};

static inline BLOCK_TYPE *gridAt(BLOCK_TYPE grid[], int size, int i, int j, int k) {
	if ( i >= 0 && i < size && j >= 0 && j < size && k >= 0 && k < Chunk::height ) {
		return &grid[i + j * size + k * size * size];
	}
	return nullptr;
}

enum class ChunkJobType {
	GenerateMesh,
	UpdateMesh
};

struct ChunkMeshObj {
	std::unique_ptr<Object> solidMesh;
	std::unique_ptr<Object> decorationMesh;
	std::unique_ptr<Object> waterMesh;
};

struct MeshData {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;	
};

struct ChunkJob {
	glm::ivec2 chunkPos;
	ChunkJobType jobType;
};

struct BlockSet {
	glm::ivec3 blockPos;
	BLOCK_TYPE blockType;
};

struct ChunkData {
	glm::ivec2 chunkPos;
	std::unique_ptr<Chunk> chunk;
	MeshData solidMeshData;
	MeshData waterMeshData;
	MeshData decorationMeshData;
	ChunkJobType jobType;
};

class World {
public:
	std::unordered_map<uint64_t, std::unique_ptr<Chunk>> chunksMap;
	std::unordered_map<uint64_t, ChunkMeshObj> chunksObject;
	std::mutex chunksMapMutex;
	
	std::unordered_set<uint64_t> generatingChunks;
	std::deque<ChunkJob> generationDeque;
	std::mutex generationMutex;
	std::queue<ChunkData> finishedQueque;
	std::mutex finishedMutex;
	std::thread generationThread;
	bool runningThread = true;

	std::unordered_map<uint64_t, std::vector<BlockSet>> blocksToSet;
	BlockAtlas blockAtlas;
	int chunkGenerationRadius = 15;

	World();
	~World();
	uint64_t getChunkKey(glm::ivec2 chunkPos);
	glm::ivec2 getChunkPos(uint64_t key);
	Chunk *getChunk(glm::ivec2 chunkPos);
	ChunkMeshObj *getChunkObj(glm::ivec2 chunkPos);
	void generationWorker();
	BLOCK_TYPE getBlockAt(glm::ivec3 worldBlockPos, Chunk *originChunk);
	bool chunkExists(glm::ivec2 chunkPos);
	void newChunk(std::unique_ptr<Chunk> chunk, glm::ivec2 chunkPos);
	void loadChunks(glm::vec3 position);
};

static bool isBlockOpaque(BLOCK_TYPE blockType) {
	switch ( blockType ) {
	case BLOCK_TYPE::AIR:
	case BLOCK_TYPE::LEAVES:
	case BLOCK_TYPE::WATER:
	case BLOCK_TYPE::SHORT_GRASS:
		return false;
	default:
		return true;
	}
}

static bool isBlockSolidAO(BLOCK_TYPE blockType) {
	switch ( blockType ) {
	case BLOCK_TYPE::AIR:
	case BLOCK_TYPE::WATER:
	case BLOCK_TYPE::SHORT_GRASS:
		return false;
	default:
		return true;
	}
}

enum class ChunkMeshType {
	SOLID,
	WATER,
	DECORATION
};
