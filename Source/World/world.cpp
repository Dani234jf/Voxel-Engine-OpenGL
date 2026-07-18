#include "../Engine/engine.h"
#include "world.h"
#include <chrono>

BlockAtlas::BlockAtlas(Texture texture) : texture(texture) {
	this->texture = texture;
}

uint32_t hash(int x, int z, int seed) {
	x = x - z * x / seed;
	uint32_t h = x * 3747992993u + z * 668265263u + seed * 1442695041u;
	h = ( h ^ ( h >> 13 ) ) * 1274126177u;
	h ^= ( h >> 16 );
	return h;
}

float randD(int x, int z) {
	int seed = 1974281;
	return ( hash(x, z, seed) & 0xFFFFFF ) / float(0xFFFFFF);
}

glm::vec2 BlockAtlas::getUVcoord(glm::ivec2 texturePos) {
	return glm::vec2(( texturePos.x * tileSize ) / (float) texture.width, ( texturePos.y * tileSize ) / (float) texture.height);
}

float BlockAtlas::getTileSize() {
	return (float) tileSize / texture.width;
}

const glm::ivec2 getTexturePos(BLOCK_TYPE blockType, BLOCK_DIR dir) {
	switch ( blockType ) {
	case BLOCK_TYPE::GRASS:
		return dir == UP ? glm::ivec2(2, 0) : dir == DOWN ? glm::ivec2(1, 0) : glm::ivec2(0, 0);
	case BLOCK_TYPE::DIRT:
		return glm::ivec2(1, 0);
	case BLOCK_TYPE::STONE:
		return glm::ivec2(3, 0);
	case BLOCK_TYPE::SAND:
		return glm::ivec2(0, 1);
	case BLOCK_TYPE::LEAVES:
		return glm::ivec2(3, 1);
	case BLOCK_TYPE::WATER:
		return glm::ivec2(0, 2);
	case BLOCK_TYPE::SHORT_GRASS:
		return glm::ivec2(1, 2);
	case BLOCK_TYPE::WOOD:
		return dir == UP || dir == DOWN ? glm::ivec2(2, 1) : glm::ivec2(1, 1);
	default:
		return glm::ivec2(0, 0);
	}
}

FastNoiseLite Chunk::plainsNoise;
FastNoiseLite Chunk::montainsNoise;
FastNoiseLite Chunk::montainsMask;
FastNoiseLite Chunk::riverNoise;
FastNoiseLite Chunk::oceanNoise;
FastNoiseLite Chunk::continentNoise;

glm::ivec2 Chunk::convertToChunkPos(glm::vec3 worldPos) {
	return glm::ivec2(floor(worldPos.x / ( blockSize * size )), floor(worldPos.z / ( blockSize * size )));
}

void Chunk::generateChunk(glm::vec3 worldPos) {
	glm::ivec2 chunkGlobalPos = convertToChunkPos(worldPos);
	getChunkBlocks(chunkGlobalPos);
	spawnTrees(chunkGlobalPos);
	spawnGrass(chunkGlobalPos);
}

inline int Chunk::generateHeight(float x, float y, float seaLevel) {

	// Planícies
	float plains = ( ( plainsNoise.GetNoise(x, y) + 1.0f ) * 0.5f );
	plains = plains * height * 0.12f + seaLevel * 1.2f;

	// Montanhas
	float mountains = ( ( montainsNoise.GetNoise(x, y) + 1.0f ) * 0.5f );
	mountains = mountains * mountains * height * 0.3f;

	mountains += montainsNoise.GetNoise(x * 3 + 1992.1f, y * 3 + 1293) * 6.0f;
	mountains += montainsNoise.GetNoise(x * 15 + 182, y * 15 + 946) * 0.5f;

	float mountainMask = ( ( montainsMask.GetNoise(x, y) + 1.0f ) * 0.5f );
	mountainMask = glm::smoothstep(0.55f, 0.75f, mountainMask);

	float terrainHeight = plains + mountains * mountainMask;

	// add more detail

	terrainHeight += plainsNoise.GetNoise(x * 10 + 192.3, y * 10 + 39481.1);

	// Continentes
	float continent = ( ( continentNoise.GetNoise(x, y) + 1.0f ) * 0.5f );

	float oceanMask = 1.0f - glm::smoothstep(0.2f, 0.6f, continent);

	// Fundo do oceano
	float oceanDetail = ( ( oceanNoise.GetNoise(x, y) + 1.0f ) * 0.5f );

	float oceanFloor = seaLevel - 50.0f - oceanDetail * 25.0f;

	return glm::mix(terrainHeight, oceanFloor, oceanMask);
}

void Chunk::gridSet(glm::ivec2 chunkPos, int i, int j, int k, BLOCK_TYPE block) {
	BLOCK_TYPE *blockType = gridAt(grid, size, i, j, k);
	if ( blockType ) {
		*blockType = block;
	}
	else {

		glm::ivec2 worldPos = glm::ivec2(chunkPos.x * size + i, chunkPos.y * size + j);
		glm::ivec2 otherChunkPos = glm::ivec2(floor((float)worldPos.x / size), floor((float)worldPos.y / size));
		glm::ivec2 localPos = worldPos % size;

		if ( localPos.x < 0 ) { localPos.x += size; }
		if ( localPos.y < 0 ) { localPos.y += size; }

		world->blocksToSet[world->getChunkKey(otherChunkPos)].push_back({glm::ivec3(localPos.x, localPos.y, k), block});
	}
}

void Chunk::initNoiseParams() {
	Chunk::plainsNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Chunk::plainsNoise.SetFrequency(0.01f);

	Chunk::montainsNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Chunk::montainsNoise.SetFrequency(0.03f);

	Chunk::montainsMask.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Chunk::montainsMask.SetFrequency(0.005f);

	Chunk::riverNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Chunk::riverNoise.SetFrequency(0.001f);

	Chunk::oceanNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Chunk::oceanNoise.SetFrequency(0.04f);

	Chunk::continentNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Chunk::continentNoise.SetFrequency(0.0005f);
}

void Chunk::getChunkBlocks(glm::ivec2 chunkPos) {
	float seaLevel = height * 0.4f;			
	int minHeight= seaLevel - 79;
	int maxHeight = 1.2f * seaLevel + 0.42f * height;

	for ( int i = 0; i < size; i++ ) {
		for ( int j = 0; j < size; j++ ) {

			float x = chunkPos.x * size + i;
			float y = chunkPos.y * size + j;


			for ( int k = 0; k < minHeight; k++ ) {
				*gridAt(grid, size, i, j, k) = BLOCK_TYPE::STONE;
			}

			for ( int k = maxHeight + 1; k < height; k++ ) {
				*gridAt(grid, size, i, j, k) = BLOCK_TYPE::AIR;
			}
			
			int gridHeight = generateHeight(x, y, seaLevel);

			for ( int k = minHeight; k <= maxHeight; k++ ) {
				if ( k > gridHeight ) {
					if ( k <= seaLevel * 1.3f ) {
						*gridAt(grid, size, i, j, k) = BLOCK_TYPE::WATER;
					}
					else {
						*gridAt(grid, size, i, j, k) = BLOCK_TYPE::AIR;
					}
				}
				else if ( k < gridHeight - 4 ) {
					*gridAt(grid, size, i, j, k) = BLOCK_TYPE::STONE;
				}
				else if ( k <= seaLevel * 1.32f ) {
					*gridAt(grid, size, i, j, k) = BLOCK_TYPE::SAND;
				}
				else {
					// Fundo do mar
					if ( gridHeight < seaLevel ) {
						*gridAt(grid, size, i, j, k) = BLOCK_TYPE::SAND;
					}
					// Praia
					else if ( gridHeight <= seaLevel + 2 ) {
						*gridAt(grid, size, i, j, k) = BLOCK_TYPE::SAND;
					}
					// Terra normal
					else if ( k == gridHeight ) {
						*gridAt(grid, size, i, j, k) = BLOCK_TYPE::GRASS;
					}
					else {
						*gridAt(grid, size, i, j, k) = BLOCK_TYPE::DIRT;
					}
				}
			}
		}
	}
}

BLOCK_TYPE Chunk::getBlock(glm::ivec3 blockPos) {

	float seaLevel = height * 0.4f;
	int minHeight = seaLevel - 79;
	int maxHeight = 1.2f * seaLevel + 0.42f * height;

	if ( blockPos.z >= 0 && blockPos.z < minHeight ) {
		return BLOCK_TYPE::STONE;
	}	

	if ( blockPos.z >= maxHeight + 1 && blockPos.z < height ) {
		return BLOCK_TYPE::AIR;
	}	
	
	int gridHeight = generateHeight(blockPos.x, blockPos.y, seaLevel);

	
	if ( blockPos.z > gridHeight ) {
		if ( blockPos.z <= seaLevel * 1.3f ) {
			return BLOCK_TYPE::WATER;
		}
		else {
			return BLOCK_TYPE::AIR;
		}
	}
	else if ( blockPos.z < gridHeight - 4 ) {
		return BLOCK_TYPE::STONE;
	}
	else if ( blockPos.z <= seaLevel * 1.32f ) {
		return BLOCK_TYPE::SAND;
	}
	else {
		// Fundo do mar
		if ( gridHeight < seaLevel ) {
			return BLOCK_TYPE::SAND;
		}
		// Praia
		else if ( gridHeight <= seaLevel + 2 ) {
			return BLOCK_TYPE::SAND;
		}
		// Terra normal
		else if ( blockPos.z == gridHeight ) {
			return BLOCK_TYPE::GRASS;
		}
		else {
			return BLOCK_TYPE::DIRT;
		}
	}
	
}

void Chunk::spawnGrass(glm::ivec2 chunkPos) {
	for ( int i = 0; i < size; i++ ) {
		for ( int j = 0; j < size; j++ ) {

			float spwanShortGrass = ( randD(chunkPos.x + i, chunkPos.y + i) + randD(i * j + height, chunkPos.x - j) ) * 0.5f;
			spwanShortGrass = randD(spwanShortGrass * i - j, spwanShortGrass - j * i) * 0.3f + spwanShortGrass * 0.3f;

			for ( int k = height - 1; k >= 0; k-- ) {
				if ( spwanShortGrass > 0.4 && *gridAt(grid, size, i, j, k) == BLOCK_TYPE::GRASS && *gridAt(grid, size, i, j, k + 1) == BLOCK_TYPE::AIR ) {
					gridSet(chunkPos, i, j, k + 1, BLOCK_TYPE::SHORT_GRASS);
					break;
				}
			}

		}
	}
}

static bool treeNearBy(glm::ivec2 globalBlockPos, Chunk *chunk, World &world, float threshold) {
	float thresholdSquared = threshold * threshold;
	for ( int i = -threshold; i <= threshold; i++ ) {
		for ( int j = -threshold; j <= threshold; j++ ) {
			
			if ( i * i + j * j > thresholdSquared ) {
				continue;
			}
			
			for ( int k = Chunk::height * 0.8f; k >= Chunk::height * 0.528f; k-- ) {
				BLOCK_TYPE blockType = world.getBlockAt(glm::ivec3(globalBlockPos.x + i, globalBlockPos.y + j, k), chunk);			
				if ( blockType == BLOCK_TYPE::WOOD ) {
					return true;
				}
			}

		}
	}
	return false;
}

void Chunk::spawnTrees(glm::ivec2 chunkPos) {

	for ( int i = 0; i < size; i++ ) {
		for ( int j = 0; j < size; j++ ) {

			float spawnTree = ( randD(chunkPos.x + i, chunkPos.y + i) + randD(i * j + height, chunkPos.x - j) ) * 0.5f;
			spawnTree = randD(spawnTree * i - j, spawnTree - j * i) * 0.3f + spawnTree * 0.7f;

			if ( spawnTree < 0.8f || treeNearBy(glm::ivec2(i, j) + chunkPos * size, this, *world, 5) ) {
				continue;
			}

			for ( int k = height - 1; k >= 0; k-- ) {
				if ( *gridAt(grid, size, i, j, k) == BLOCK_TYPE::GRASS ) {

					for ( int ti = -2; ti <= 2; ti++ ) {
						for ( int tj = -2; tj <= 2; tj++ ) {
							for ( int tk = 0; tk <= 2; tk++ ) {

								int _i = i + ti;
								int _j = j + tj;
								int _k = k + 4 + tk;

								gridSet(chunkPos, _i, _j, _k, BLOCK_TYPE::LEAVES);
							}
						}
					}

					for ( int t = 1; t <= 5; t++ ) {
						*gridAt(grid, size, i, j, k + t) = BLOCK_TYPE::WOOD;
					}

					gridSet(chunkPos, i, j, k + 7, BLOCK_TYPE::LEAVES);
					gridSet(chunkPos, i + 1, j, k + 7, BLOCK_TYPE::LEAVES);
					gridSet(chunkPos, i - 1, j, k + 7, BLOCK_TYPE::LEAVES);
					gridSet(chunkPos, i, j + 1, k + 7, BLOCK_TYPE::LEAVES);
					gridSet(chunkPos, i, j - 1, k + 7, BLOCK_TYPE::LEAVES);
					
					gridSet(chunkPos, i - 2, j - 2, k + 6, BLOCK_TYPE::AIR);
					gridSet(chunkPos, i + 2, j + 2, k + 6, BLOCK_TYPE::AIR);
					gridSet(chunkPos, i + 2, j - 2, k + 6, BLOCK_TYPE::AIR);
					gridSet(chunkPos, i - 2, j + 2, k + 6, BLOCK_TYPE::AIR);

					break;
				}
			}
		}
	}

}

float Chunk::vertexAO(bool side0, bool side1, bool corner) {
	if ( side0 && side1 ) {
		return 0.6f;
	}
	return 1.0f - ( side0 + side1 + corner ) * 0.133f;
}

void Chunk::addFace(int dir, glm::vec3 pos, std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, glm::vec2 baseUV, float tileSize, glm::ivec2 chunkPos) {
	glm::vec3 normal;
	glm::vec3 base = pos * blockSize;

	glm::vec3 v0, v1, v2, v3;
	glm::vec2 u0, u1, u2, u3;
	float ao[4] = {1, 1, 1, 1};

	if ( dir == 0 || dir == 1 )
	{
		normal = glm::vec3(dir == 0 ? -1.0f : 1.0f, 0.0f, 0.0f);

		float x = base.x + ( dir == 1 ? blockSize : 0.0f );

		v0 = glm::vec3(x, base.y, base.z);
		v1 = glm::vec3(x, base.y, base.z + blockSize);
		v2 = glm::vec3(x, base.y + blockSize, base.z + blockSize);
		v3 = glm::vec3(x, base.y + blockSize, base.z);

		u0 = glm::vec2(baseUV.x + tileSize, baseUV.y + tileSize);
		u1 = glm::vec2(baseUV.x, baseUV.y + tileSize);
		u2 = glm::vec2(baseUV.x, baseUV.y);
		u3 = glm::vec2(baseUV.x + tileSize, baseUV.y);
	}
	else if ( dir == 2 || dir == 3 )
	{
		normal = glm::vec3(0.0f, 0.0f, dir == 2 ? -1.0f : 1.0f);

		float z = base.z + ( dir == 3 ? blockSize : 0.0f );

		v0 = glm::vec3(base.x, base.y, z);
		v1 = glm::vec3(base.x, base.y + blockSize, z);
		v2 = glm::vec3(base.x + blockSize, base.y + blockSize, z);
		v3 = glm::vec3(base.x + blockSize, base.y, z);

		u0 = glm::vec2(baseUV.x, baseUV.y + tileSize);
		u1 = glm::vec2(baseUV.x, baseUV.y);
		u2 = glm::vec2(baseUV.x + tileSize, baseUV.y);
		u3 = glm::vec2(baseUV.x + tileSize, baseUV.y + tileSize);
	}
	else if ( dir == 4 || dir == 5 )
	{
		normal = glm::vec3(0.0f, dir == 4 ? -1.0f : 1.0f, 0.0f);

		if ( dir == 5 && *gridAt(grid, size, pos.x, pos.z, pos.y) == BLOCK_TYPE::LEAVES ) {
			BLOCK_TYPE *blockAboveType = gridAt(grid, size, pos.x, pos.z, pos.y + 1);
			if ( blockAboveType && *blockAboveType == BLOCK_TYPE::LEAVES ) {
				normal.x = 0.5;
				normal.y = 0.5;
			}
		}

		float y = base.y + ( dir == 5 ? blockSize : 0.0f );

		v0 = glm::vec3(base.x, y, base.z);
		v1 = glm::vec3(base.x + blockSize, y, base.z);
		v2 = glm::vec3(base.x + blockSize, y, base.z + blockSize);
		v3 = glm::vec3(base.x, y, base.z + blockSize);

		u0 = glm::vec2(baseUV.x, baseUV.y);
		u1 = glm::vec2(baseUV.x + tileSize, baseUV.y);
		u2 = glm::vec2(baseUV.x + tileSize, baseUV.y + tileSize);
		u3 = glm::vec2(baseUV.x, baseUV.y + tileSize);
	}

	for ( int v = 0; v < 4; v++ ) {

		AODir offset = aoMap[dir][v];

		bool side0 = isSolidAO(pos.x + offset.side0.x, pos.z + offset.side0.z, pos.y + offset.side0.y, chunkPos);
		bool side1 = isSolidAO(pos.x + offset.side1.x, pos.z + offset.side1.z, pos.y + offset.side1.y, chunkPos);
		bool corner = isSolidAO(pos.x + offset.corner.x, pos.z + offset.corner.z, pos.y + offset.corner.y, chunkPos);

		ao[v] = vertexAO(side0, side1, corner);

	}

	unsigned int firstIndex = (unsigned int) vertices.size();
	
	vertices.push_back({ v0, normal, u0, ao[0] });
	vertices.push_back({ v1, normal, u1, ao[1] });
	vertices.push_back({ v2, normal, u2, ao[2] });
	vertices.push_back({ v3, normal, u3, ao[3] });

	if ( dir == 0 || dir == 2 || dir == 4 ) {
		if ( ao[0] + ao[2] > ao[1] + ao[3] ) {
			indices.push_back(firstIndex + 0);
			indices.push_back(firstIndex + 1);
			indices.push_back(firstIndex + 2);
			indices.push_back(firstIndex + 0);
			indices.push_back(firstIndex + 2);
			indices.push_back(firstIndex + 3);
		}
		else {
			indices.push_back(firstIndex + 0);
			indices.push_back(firstIndex + 1);
			indices.push_back(firstIndex + 3);
			indices.push_back(firstIndex + 1);
			indices.push_back(firstIndex + 2);
			indices.push_back(firstIndex + 3);
		}
	}
	else {
		if ( ao[0] + ao[2] > ao[1] + ao[3] ) {
			indices.push_back(firstIndex + 0);
			indices.push_back(firstIndex + 2);
			indices.push_back(firstIndex + 1);
			indices.push_back(firstIndex + 0);
			indices.push_back(firstIndex + 3);
			indices.push_back(firstIndex + 2);
		}
		else {
			indices.push_back(firstIndex + 0);
			indices.push_back(firstIndex + 3);
			indices.push_back(firstIndex + 1);
			indices.push_back(firstIndex + 3);
			indices.push_back(firstIndex + 2);
			indices.push_back(firstIndex + 1);
		}
	}
}

void Chunk::updateWater() {
	for ( int i = 0; i < size; i++ ) {
		for ( int j = 0; j < size; j++ ) {
			for ( int k = 0; k < height; k++ ) {
				if ( *gridAt(grid, size, i, j, k) != BLOCK_TYPE::WATER ) {
					continue;
				}

				BLOCK_TYPE *blockType = gridAt(grid, size, i + 1, j, k);
				if ( blockType != nullptr && *blockType == BLOCK_TYPE::AIR ) {
					*gridAt(grid, size, i + 1, j, k) = BLOCK_TYPE::WATER;
				}
				blockType = gridAt(grid, size, i, j + 1, k);
				if ( blockType != nullptr && *blockType == BLOCK_TYPE::AIR ) {
					*gridAt(grid, size, i, j + 1, k) = BLOCK_TYPE::WATER;
				}
				blockType = gridAt(grid, size, i - 1, j, k);
				if ( blockType != nullptr && *blockType == BLOCK_TYPE::AIR ) {
					*gridAt(grid, size, i - 1, j, k) = BLOCK_TYPE::WATER;
				}
				blockType = gridAt(grid, size, i, j - 1, k);
				if ( blockType != nullptr && *blockType == BLOCK_TYPE::AIR ) {
					*gridAt(grid, size, i, j - 1, k) = BLOCK_TYPE::WATER;
				}
				blockType = gridAt(grid, size, i, j, k - 1);
				if ( blockType != nullptr && *blockType == BLOCK_TYPE::AIR ) {
					*gridAt(grid, size, i, j, k - 1) = BLOCK_TYPE::WATER;
				}
			}
		}
	}
}

void Chunk::generateBlockMesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, glm::ivec2 chunkPos, BlockAtlas &blockAtlas, bool isWater) {
	glm::vec2 baseUV;
	glm::vec3 pos;

	vertices.reserve(30000);
	indices.reserve(10000);

	for ( int i = 0; i < size; i++ ) {
		for ( int j = 0; j < size; j++ ) {
			for ( int k = 0; k < height; k++ ) {
				BLOCK_TYPE blockType = *gridAt(grid, size, i, j, k);
				
				if ( blockType == BLOCK_TYPE::AIR || blockType == BLOCK_TYPE::SHORT_GRASS ) {
					continue;
				}

				if ( isWater ) {
					if ( blockType != BLOCK_TYPE::WATER ) {
						continue;
					}
				}
				else if ( blockType == BLOCK_TYPE::WATER ) {
					continue;
				}

				baseUV = blockAtlas.getUVcoord(getTexturePos(blockType, DEFAULT));
				pos = glm::vec3(i, k, j);

				if ( !isPosOpaque(i - 1, j, k, chunkPos, isWater) ) { // -X +X
					addFace(0, pos, vertices, indices, baseUV, blockAtlas.getTileSize(), chunkPos);
				}
				if ( !isPosOpaque(i + 1, j, k, chunkPos, isWater) ) {
					addFace(1, pos, vertices, indices, baseUV, blockAtlas.getTileSize(), chunkPos);
				}
				if ( !isPosOpaque(i, j - 1, k, chunkPos, isWater) ) { // -Z +Z
					addFace(2, pos, vertices, indices, baseUV, blockAtlas.getTileSize(), chunkPos);
				}
				if ( !isPosOpaque(i, j + 1, k, chunkPos, isWater) ) {
					addFace(3, pos, vertices, indices, baseUV, blockAtlas.getTileSize(), chunkPos);
				}
				if ( !isPosOpaque(i, j, k - 1, chunkPos, isWater) ) { // -Y +Y
					baseUV = blockAtlas.getUVcoord(getTexturePos(blockType, DOWN));
					addFace(4, pos, vertices, indices, baseUV, blockAtlas.getTileSize(), chunkPos);
				}
				if ( !isPosOpaque(i, j, k + 1, chunkPos, isWater) ) {
					baseUV = blockAtlas.getUVcoord(getTexturePos(blockType, UP));
					addFace(5, pos, vertices, indices, baseUV, blockAtlas.getTileSize(), chunkPos);
				}
			}
		}
	}
}

void Chunk::generateDecorationMesh(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices, BlockAtlas &blockAtlas) {
	for ( int i = 0; i < size; i++ ) {
		for ( int j = 0; j < size; j++ ) {
			for ( int k = 0; k < height; k++ ) {
				if ( *gridAt(grid, size, i, j, k) != BLOCK_TYPE::SHORT_GRASS ) {
					continue;
				}

				glm::vec2 baseUV = blockAtlas.getUVcoord(getTexturePos(*gridAt(grid, size, i, j, k), DEFAULT));
				float tileSize = blockAtlas.getTileSize();
				glm::vec3 grassOffset = glm::vec3(blockSize * 0.15f, 0, blockSize * 0.15f);

				glm::vec3 v0, v1, v2, v3;
				glm::vec2 u0, u1, u2, u3;
				float ao0 = 0.9f, ao1 = 0.9f, ao2 = 1.0f, ao3 = 1.0f;

				glm::vec3 normal = glm::vec3(-1, 1, 1);
				glm::vec3 base = glm::vec3(i, k, j) * blockSize;

				v0 = glm::vec3(base.x, base.y, base.z) + grassOffset;
				v1 = glm::vec3(base.x + blockSize, base.y, base.z + blockSize) - grassOffset;
				v2 = glm::vec3(base.x + blockSize, base.y + blockSize * 0.9f, base.z + blockSize) - grassOffset;
				v3 = glm::vec3(base.x, base.y + blockSize * 0.9f, base.z) + grassOffset;

				u0 = glm::vec2(baseUV.x, baseUV.y + tileSize);
				u1 = glm::vec2(baseUV.x + tileSize, baseUV.y + tileSize);
				u2 = glm::vec2(baseUV.x + tileSize, baseUV.y);
				u3 = glm::vec2(baseUV.x, baseUV.y);

				unsigned int firstIndex = (unsigned int) vertices.size();

				vertices.push_back({ v0, normal, u0, ao0 });
				vertices.push_back({ v1, normal, u1, ao1 });
				vertices.push_back({ v2, normal, u2, ao2 });
				vertices.push_back({ v3, normal, u3, ao3 });

				indices.push_back(firstIndex + 0);
				indices.push_back(firstIndex + 1);
				indices.push_back(firstIndex + 2);
				indices.push_back(firstIndex + 0);
				indices.push_back(firstIndex + 2);
				indices.push_back(firstIndex + 3);

				grassOffset.z = -grassOffset.z;
				normal = glm::normalize(glm::vec3(1, 1, 1));

				v0 = glm::vec3(base.x, base.y, base.z + blockSize) + grassOffset;
				v1 = glm::vec3(base.x + blockSize, base.y, base.z) - grassOffset;
				v2 = glm::vec3(base.x + blockSize, base.y + blockSize * 0.9f, base.z) - grassOffset;
				v3 = glm::vec3(base.x, base.y + blockSize * 0.9f, base.z + blockSize) + grassOffset;

				u0 = glm::vec2(baseUV.x, baseUV.y + tileSize);
				u1 = glm::vec2(baseUV.x + tileSize, baseUV.y + tileSize);
				u2 = glm::vec2(baseUV.x + tileSize, baseUV.y);
				u3 = glm::vec2(baseUV.x, baseUV.y);

				firstIndex = (unsigned int) vertices.size();

				vertices.push_back({ v0, normal, u0, ao0 });
				vertices.push_back({ v1, normal, u1, ao1 });
				vertices.push_back({ v2, normal, u2, ao2 });
				vertices.push_back({ v3, normal, u3, ao3 });

				indices.push_back(firstIndex + 0);
				indices.push_back(firstIndex + 1);
				indices.push_back(firstIndex + 2);
				indices.push_back(firstIndex + 0);
				indices.push_back(firstIndex + 2);
				indices.push_back(firstIndex + 3);
			}
		}
	}
}

bool Chunk::isPosOpaque(int i, int j, int k, glm::ivec2 chunkPos, bool isWater) {
	
	if ( k >= height || k < 0 ) {
		return true;
	}

	if ( i >= size || i < 0 || j >= size || j < 0 ) {
		glm::ivec3 blockPos = glm::ivec3(chunkPos.x * size + i, chunkPos.y * size + j, k);
		glm::ivec2 neighboorChunkPos = glm::ivec2(blockPos.x >> 5, blockPos.y >> 5);
		glm::ivec3 blockPosLocal = glm::ivec3(blockPos.x % size, blockPos.y % size, k);

		blockPosLocal.x = blockPosLocal.x < 0 ? blockPosLocal.x + size : blockPosLocal.x;
		blockPosLocal.y = blockPosLocal.y < 0 ? blockPosLocal.y + size : blockPosLocal.y;

		Chunk *neighboorChunk = world->getChunk(neighboorChunkPos);

		if ( !neighboorChunk ) {
			return !isWater ? isBlockOpaque(getBlock(blockPos)) : getBlock(blockPos) == BLOCK_TYPE::WATER;
		}

		BLOCK_TYPE blockType = *gridAt(neighboorChunk->grid, size, blockPosLocal.x, blockPosLocal.y, blockPosLocal.z);
		return !isWater ? isBlockOpaque(blockType) : blockType == BLOCK_TYPE::WATER;
	}

	return !isWater ? isBlockOpaque(*gridAt(grid, size, i, j, k)) : *gridAt(grid, size, i, j, k) == BLOCK_TYPE::WATER;
}

BLOCK_TYPE World::getBlockAt(glm::ivec3 blockPos, Chunk *originChunk) {

	if ( blockPos.z >= Chunk::height || blockPos.z < 0 ) {
		return BLOCK_TYPE::NONE;
	}
	
	glm::ivec3 blockPosLocal = glm::ivec3(blockPos.x % Chunk::size, blockPos.y % Chunk::size, blockPos.z);

	blockPosLocal.x = blockPosLocal.x < 0 ? blockPosLocal.x + Chunk::size : blockPosLocal.x;
	blockPosLocal.y = blockPosLocal.y < 0 ? blockPosLocal.y + Chunk::size : blockPosLocal.y;
	
	if ( blockPosLocal.x >= Chunk::size || blockPosLocal.x < 0 || blockPosLocal.y >= Chunk::size || blockPosLocal.y < 0 || !originChunk ) {
		glm::ivec2 chunkPos = glm::ivec2(blockPos.x >> 5, blockPos.y >> 5);
		Chunk *chunk = getChunk(chunkPos);

		if ( !chunk ) {
			return Chunk::getBlock(blockPos);
		}
		
		return *gridAt(chunk->grid, Chunk::size, blockPosLocal.x, blockPosLocal.y, blockPosLocal.z);
	}

	return *gridAt(originChunk->grid, Chunk::size, blockPosLocal.x, blockPosLocal.y, blockPosLocal.z);
}

bool Chunk::isSolidAO(int i, int j, int k, glm::ivec2 chunkPos) {

	if ( k >= height || k < 0 ) {
		return true;
	}

	if ( i >= size || i < 0 || j >= size || j < 0 ) {
		glm::ivec3 blockPos = glm::ivec3(chunkPos.x * size + i, chunkPos.y * size + j, k);
		glm::ivec2 neighboorChunkPos = glm::ivec2(blockPos.x >> 5, blockPos.y >> 5);
		glm::ivec3 blockPosLocal = glm::ivec3(blockPos.x % size, blockPos.y % size, k);

		blockPosLocal.x = blockPosLocal.x < 0 ? blockPosLocal.x + size : blockPosLocal.x;
		blockPosLocal.y = blockPosLocal.y < 0 ? blockPosLocal.y + size : blockPosLocal.y;

		Chunk *neighboorChunk = world->getChunk(neighboorChunkPos);

		if ( !neighboorChunk ) {
			return isBlockSolidAO(getBlock(blockPos));
		}

		BLOCK_TYPE blockType = *gridAt(neighboorChunk->grid, size, blockPosLocal.x, blockPosLocal.y, blockPosLocal.z);
		return isBlockSolidAO(blockType);
	}

	return isBlockSolidAO(*gridAt(grid, size, i, j, k));
}

uint64_t World::getChunkKey(glm::ivec2 chunkPos) {
	return ( uint64_t((uint32_t) chunkPos.x) << 32 ) | (uint32_t) chunkPos.y;
}

Chunk *World::getChunk(glm::ivec2 chunkPos) {
	std::lock_guard<std::mutex> lock(chunksMapMutex);
	auto it = chunksMap.find(getChunkKey(chunkPos));
	return it == chunksMap.end() ? nullptr : it->second.get();
}

ChunkMeshObj *World::getChunkObj(glm::ivec2 chunkPos) {
	auto it = chunksObject.find(getChunkKey(chunkPos));
	return it == chunksObject.end() ? nullptr : &it->second;
}

World::World() {
	generationThread = std::thread(&World::generationWorker, this);
}

bool World::chunkExists(glm::ivec2 chunkPos) {
	std::lock_guard<std::mutex> lock(chunksMapMutex);
	auto it = chunksMap.find(getChunkKey(chunkPos));
	return it != chunksMap.end();
}

void World::newChunk(std::unique_ptr<Chunk> chunk, glm::ivec2 chunkPos) {
	std::lock_guard<std::mutex> lock(chunksMapMutex);
	chunksMap[getChunkKey(chunkPos)] = std::move(chunk);
}


void World::loadChunks(glm::vec3 position) {

	float distanceSquared = chunkGenerationRadius * chunkGenerationRadius;
	glm::ivec2 deltaPos = glm::ivec2(0, 0);
	glm::ivec2 dir[] = { {1, 0}, {0, -1}, {-1, 0}, {0, 1} };
	int dt = 0;

	for ( int i = -1; i <= 1; i++ ) {
		for ( int j = -1; j <= 1; j++ ) {

			glm::ivec2 chunkPos = Chunk::convertToChunkPos(position) + glm::ivec2(i,j);
			uint64_t chunkKey = getChunkKey(chunkPos);

			if ( !chunkExists(chunkPos) && generatingChunks.find(chunkKey) == generatingChunks.end() ) {
				{
					std::lock_guard<std::mutex> lock(generationMutex);
					generationDeque.push_back({chunkPos, ChunkJobType::GenerateMesh});
					generatingChunks.insert(chunkKey);
				}
			}
		}
	}	

	for ( int i = 0; i < distanceSquared; i++ ) {
		if ( i % 2 == 0 ) {
			dt++;
		}
		for ( int t = 0; t <= dt; t++ ) {
			if ( deltaPos.x == chunkGenerationRadius / 2 && deltaPos.y == chunkGenerationRadius / 2 ) {
				break;
			}
			deltaPos += dir[i % 4];
			
			if ( deltaPos.x * deltaPos.x + deltaPos.y * deltaPos.y > distanceSquared ) {
				continue;
			}

			glm::ivec2 chunkPos = Chunk::convertToChunkPos(position) + deltaPos;
			uint64_t chunkKey = getChunkKey(chunkPos);

			if ( !chunkExists(chunkPos) && generatingChunks.find(chunkKey) == generatingChunks.end() ) {
				{
					std::lock_guard<std::mutex> lock(generationMutex);
					generationDeque.push_back({chunkPos, ChunkJobType::GenerateMesh});
					generatingChunks.insert(chunkKey);
				}
			}

		}
	}

	int processed = 0;

	while ( processed < 8 ) {
		bool finishedQuequeEmpty;
		{
			std::lock_guard<std::mutex> lock(finishedMutex);
			finishedQuequeEmpty = finishedQueque.empty();
		}

		if ( finishedQuequeEmpty ) {
			break;
		}
		
		ChunkData chunkData;

		{
			std::lock_guard<std::mutex> lock(finishedMutex);
			chunkData = std::move(finishedQueque.front());
			finishedQueque.pop();
		}

		processed++;
		if ( chunkData.jobType == ChunkJobType::GenerateMesh ) {

			glm::vec3 worldPos = glm::vec3(chunkData.chunkPos.x * Chunk::blockSize * Chunk::size, 0, chunkData.chunkPos.y * Chunk::blockSize * Chunk::size);

			std::unique_ptr<Object> chunkSolidObj = std::make_unique<Object>();
			chunkSolidObj->addComponent<Transform>();
			chunkSolidObj->getComponent<Transform>()->position = worldPos;
			chunkSolidObj->getComponent<Transform>()->isStatic = true;
			chunkSolidObj->addComponent<Mesh>(chunkData.solidMeshData.vertices, chunkData.solidMeshData.indices);

			std::unique_ptr<Object> chunkDecObj = std::make_unique<Object>();
			chunkDecObj->addComponent<Mesh>(chunkData.decorationMeshData.vertices, chunkData.decorationMeshData.indices);

			newChunk(std::move(chunkData.chunk), chunkData.chunkPos);

			std::unique_ptr<Object> chunkWaterObj = std::make_unique<Object>();
			chunkWaterObj->addComponent<Mesh>(chunkData.waterMeshData.vertices, chunkData.waterMeshData.indices);

			chunksObject[getChunkKey(chunkData.chunkPos)] = { std::move(chunkSolidObj), std::move(chunkDecObj), std::move(chunkWaterObj) };

			generatingChunks.erase(getChunkKey(chunkData.chunkPos));
			
			continue;
		}

		ChunkMeshObj *chunkMeshObj = getChunkObj(chunkData.chunkPos);

		chunkMeshObj->decorationMesh->getComponent<Mesh>()->updateMesh(chunkData.decorationMeshData.vertices, chunkData.decorationMeshData.indices);
		chunkMeshObj->solidMesh->getComponent<Mesh>()->updateMesh(chunkData.solidMeshData.vertices, chunkData.solidMeshData.indices);
		chunkMeshObj->waterMesh->getComponent<Mesh>()->updateMesh(chunkData.waterMeshData.vertices, chunkData.waterMeshData.indices);		
		
	}

}

World::~World() {
	runningThread = false;

	if ( generationThread.joinable() ) {
		generationThread.join();
	}
}

glm::ivec2 World::getChunkPos(uint64_t key) {
	return glm::ivec2(
		int32_t(key >> 32),
		int32_t(key & 0xFFFFFFFF)
	);
}

void World::generationWorker() {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	while ( runningThread ) {

		for ( auto it = blocksToSet.begin(); it != blocksToSet.end(); ) {
			glm::ivec2 chunkPos = getChunkPos(it->first);
			Chunk *chunk = getChunk(chunkPos);
			
			if ( !chunk ) {
				it++;
				continue;
			}

			for ( const auto &blockSet : it->second ) {
				*gridAt(chunk->grid, Chunk::size, blockSet.blockPos.x, blockSet.blockPos.y, blockSet.blockPos.z) = blockSet.blockType;
			}

			{
				std::lock_guard<std::mutex> lock(generationMutex);
				generationDeque.push_back({chunkPos, ChunkJobType::UpdateMesh});
			}

			it = blocksToSet.erase(it);
		}
		
		for ( int i = 0; i < 2; i++ ) {

			{
				std::lock_guard<std::mutex> lock(generationMutex);
				if ( generationDeque.empty() ) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
			}

			ChunkJob chunkJob;

			{
				std::lock_guard<std::mutex> lock(generationMutex);
				chunkJob = generationDeque.front();
				generationDeque.pop_front();
			}

			if ( chunkJob.jobType == ChunkJobType::GenerateMesh ) {

				glm::vec3 worldPos = glm::vec3(chunkJob.chunkPos.x * Chunk::blockSize * Chunk::size, 0, chunkJob.chunkPos.y * Chunk::blockSize * Chunk::size);

				std::unique_ptr<Chunk> chunk = std::make_unique<Chunk>();
				chunk->world = this;
				chunk->generateChunk(worldPos);

				ChunkData chunkData;
				
				chunk->generateBlockMesh(chunkData.solidMeshData.vertices, chunkData.solidMeshData.indices, chunkJob.chunkPos, blockAtlas, false);
				chunk->generateDecorationMesh(chunkData.decorationMeshData.vertices, chunkData.decorationMeshData.indices, blockAtlas);
				chunk->generateBlockMesh(chunkData.waterMeshData.vertices, chunkData.waterMeshData.indices, chunkJob.chunkPos, blockAtlas, true);

				chunkData.chunkPos = chunkJob.chunkPos;
				chunkData.chunk = std::move(chunk);
				chunkData.jobType = chunkJob.jobType;

				{
					std::lock_guard<std::mutex> lock(finishedMutex);
					finishedQueque.push(std::move(chunkData));
				}
				continue;
			}

			Chunk *chunk = getChunk(chunkJob.chunkPos);

			if ( !chunk ) {
				continue;
			}

			ChunkData chunkData;

			chunk->generateBlockMesh(chunkData.solidMeshData.vertices, chunkData.solidMeshData.indices, chunkJob.chunkPos, blockAtlas, false);
			chunk->generateDecorationMesh(chunkData.decorationMeshData.vertices, chunkData.decorationMeshData.indices, blockAtlas);
			chunk->updateWater();
			chunk->generateBlockMesh(chunkData.waterMeshData.vertices, chunkData.waterMeshData.indices, chunkJob.chunkPos, blockAtlas, true);
			chunkData.chunk = nullptr;
			chunkData.chunkPos = chunkJob.chunkPos;
			chunkData.jobType = chunkJob.jobType;

			{
				std::lock_guard<std::mutex> lock(finishedMutex);
				finishedQueque.push(std::move(chunkData));
			}
		}
	}
}
