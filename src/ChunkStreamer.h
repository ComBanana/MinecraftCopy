#pragma once

#include "TerrainGenerator.h"

#include <cstdlib>

class ChunkStreamer
{
public:
    static bool update(
        World& world,
        const TerrainGenerator& terrainGenerator,
        int blockX,
        int blockZ,
        int renderDistance)
    {
        if (renderDistance < 0 || world.getType() != WorldType::Infinite)
            return false;

        const ChunkPos center{
            WorldCoordinates::floorDiv(blockX, WorldConfig::CHUNK_SIZE),
            WorldCoordinates::floorDiv(blockZ, WorldConfig::CHUNK_SIZE)};
        bool changed = false;

        for (int x = center.x - renderDistance;
             x <= center.x + renderDistance;
             ++x)
        {
            for (int z = center.z - renderDistance;
                 z <= center.z + renderDistance;
                 ++z)
            {
                const ChunkPos chunkPos{x, z};
                if (!world.getChunk(chunkPos))
                {
                    terrainGenerator.generateChunk(world, chunkPos);
                    changed = true;
                }
            }
        }

        const int unloadDistance = renderDistance + 1;
        for (auto it = world.getChunks().begin(); it != world.getChunks().end();)
        {
            if (std::abs(it->first.x - center.x) > unloadDistance ||
                std::abs(it->first.z - center.z) > unloadDistance)
            {
                const ChunkPos oldPos = it->first;
                ++it;
                world.removeChunk(oldPos);
                changed = true;
            }
            else
            {
                ++it;
            }
        }

        return changed;
    }
};
