
#pragma once

#include "World.h"

#include <glm/glm.hpp>

#include <vector>

struct ChunkVertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;
};

class ChunkMesher
{
public:
    static std::vector<ChunkVertex> buildMesh(
        const World& world,
        const ChunkPos& chunkPos)
    {
        std::vector<ChunkVertex> vertices;

        const Chunk* chunk = world.getChunk(chunkPos);

        if (chunk == nullptr)
            return vertices;

        vertices.reserve(4096);

        const int startX =
            chunkPos.x * WorldConfig::CHUNK_SIZE;
        const int startZ =
            chunkPos.z * WorldConfig::CHUNK_SIZE;

        for (int y = 0; y < WorldConfig::WORLD_HEIGHT; ++y)
        {
            for (int z = 0; z < WorldConfig::CHUNK_SIZE; ++z)
            {
                for (int x = 0; x < WorldConfig::CHUNK_SIZE; ++x)
                {
                    const BlockId block = chunk->getBlock(x, y, z);

                    if (block == BlockId::Air)
                        continue;

                    const int wx = startX + x;
                    const int wz = startZ + z;

                    const glm::vec3 p(
                        static_cast<float>(wx),
                        static_cast<float>(y),
                        static_cast<float>(wz));

                    // Only emit faces bordering air.
                    if (world.getBlock({wx, y + 1, wz}) == BlockId::Air)
                    {
                        addFace(vertices,
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 1, 0),
                            topTile(block));
                    }

                    if (world.getBlock({wx, y - 1, wz}) == BlockId::Air)
                    {
                        addFace(vertices,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(1, 0, 0),
                            p + glm::vec3(1, 0, 1),
                            p + glm::vec3(0, 0, 1),
                            bottomTile(block));
                    }

                    if (world.getBlock({wx + 1, y, wz}) == BlockId::Air)
                    {
                        addFace(vertices,
                            p + glm::vec3(1, 0, 0),
                            p + glm::vec3(1, 1, 0),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 0, 1),
                            sideTile(block));
                    }

                    if (world.getBlock({wx - 1, y, wz}) == BlockId::Air)
                    {
                        addFace(vertices,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(0, 0, 1),
                            sideTile(block));
                    }

                    if (world.getBlock({wx, y, wz + 1}) == BlockId::Air)
                    {
                        addFace(vertices,
                            p + glm::vec3(0, 0, 1),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 0, 1),
                            sideTile(block));
                    }

                    if (world.getBlock({wx, y, wz - 1}) == BlockId::Air)
                    {
                        addFace(vertices,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(1, 1, 0),
                            p + glm::vec3(1, 0, 0),
                            sideTile(block));
                    }
                }
            }
        }

        return vertices;
    }

private:
    static int topTile(BlockId block)
    {
        switch (block)
        {
        case BlockId::Grass: return 3;
        case BlockId::Dirt:  return 1;
        case BlockId::Stone: return 0;
        default:             return 0;
        }
    }

    static int bottomTile(BlockId block)
    {
        if (block == BlockId::Grass)
            return 1;

        return topTile(block);
    }

    static int sideTile(BlockId block)
    {
        if (block == BlockId::Grass)
            return 2;

        return topTile(block);
    }

    static void addFace(
        std::vector<ChunkVertex>& vertices,
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        const glm::vec3& d,
        int tile)
    {
        // Existing atlas: four 16x16 tiles in a 64x16 image.
        constexpr float atlasWidth = 64.0f;
        constexpr float atlasHeight = 16.0f;
        constexpr float tileSize = 16.0f;

        const float u0 =
            (tile * tileSize + 0.5f) / atlasWidth;
        const float u1 =
            ((tile + 1) * tileSize - 0.5f) / atlasWidth;
        const float v0 = 0.5f / atlasHeight;
        const float v1 =
            (tileSize - 0.5f) / atlasHeight;

        const glm::vec3 color(1.0f);

        const glm::vec2 uvA(u1, v1);
        const glm::vec2 uvB(u1, v0);
        const glm::vec2 uvC(u0, v0);
        const glm::vec2 uvD(u0, v1);

        vertices.push_back({a, color, uvA});
        vertices.push_back({b, color, uvB});
        vertices.push_back({c, color, uvC});

        vertices.push_back({a, color, uvA});
        vertices.push_back({c, color, uvC});
        vertices.push_back({d, color, uvD});
    }
};
