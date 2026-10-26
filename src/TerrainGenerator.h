
#pragma once

#include "World.h"

#include <algorithm>
#include <cstdint>

class TerrainGenerator
{
public:
    explicit TerrainGenerator(std::uint32_t seed = 12345)
        : worldSeed(seed)
    {
    }

    void generateChunk(World& world, const ChunkPos& chunkPos) const
    {
        // Ensure the chunk exists before generating its blocks.
        world.ensureChunk(chunkPos);

        const int startX =
            chunkPos.x * WorldConfig::CHUNK_SIZE;
        const int startZ =
            chunkPos.z * WorldConfig::CHUNK_SIZE;

        for (int localX = 0;
             localX < WorldConfig::CHUNK_SIZE;
             ++localX)
        {
            for (int localZ = 0;
                 localZ < WorldConfig::CHUNK_SIZE;
                 ++localZ)
            {
                const int worldX = startX + localX;
                const int worldZ = startZ + localZ;

                if (!world.isInsideWorld(worldX, worldZ))
                    continue;

                const int height = terrainHeight(worldX, worldZ);

                for (int y = 0; y < height; ++y)
                {
                    BlockId block;

                    if (y == height - 1)
                    {
                        block = BlockId::Grass;
                    }
                    else if (y >= height - 4)
                    {
                        block = BlockId::Dirt;
                    }
                    else
                    {
                        block = BlockId::Stone;
                    }

                    world.setGeneratedBlock(
                        {worldX, y, worldZ},
                        block);
                }
            }
        }

        world.applyBlockOverrides(chunkPos);
    }

private:
    std::uint32_t worldSeed;

    static std::uint32_t hashCoordinates(
        int x,
        int z,
        std::uint32_t seed)
    {
        std::uint32_t h = seed;

        h ^= static_cast<std::uint32_t>(x) * 0x8DA6B343u;
        h ^= static_cast<std::uint32_t>(z) * 0xD8163841u;

        h ^= h >> 16;
        h *= 0x7FEB352Du;
        h ^= h >> 15;
        h *= 0x846CA68Bu;
        h ^= h >> 16;

        return h;
    }

    int terrainHeight(int x, int z) const
    {
        const float broadShape = valueNoise(x, z, 48);
        const float localShape = valueNoise(x, z, 16);
        const int height = 5 + static_cast<int>(broadShape * 14.0f + localShape * 4.0f);

        return std::clamp(
            height,
            1,
            WorldConfig::WORLD_HEIGHT - 1);
    }

    static float smoothStep(float value)
    {
        return value * value * (3.0f - 2.0f * value);
    }

    float latticeValue(int x, int z) const
    {
        return static_cast<float>(hashCoordinates(x, z, worldSeed) % 10000u) /
               10000.0f;
    }

    float valueNoise(int x, int z, int scale) const
    {
        const int cellX = WorldCoordinates::floorDiv(x, scale);
        const int cellZ = WorldCoordinates::floorDiv(z, scale);
        const float localX = static_cast<float>(
            WorldCoordinates::positiveMod(x, scale)) / scale;
        const float localZ = static_cast<float>(
            WorldCoordinates::positiveMod(z, scale)) / scale;
        const float blendX = smoothStep(localX);
        const float blendZ = smoothStep(localZ);

        const float top = latticeValue(cellX, cellZ) * (1.0f - blendX) +
                          latticeValue(cellX + 1, cellZ) * blendX;
        const float bottom = latticeValue(cellX, cellZ + 1) * (1.0f - blendX) +
                             latticeValue(cellX + 1, cellZ + 1) * blendX;
        return top * (1.0f - blendZ) + bottom * blendZ;
    }
};
