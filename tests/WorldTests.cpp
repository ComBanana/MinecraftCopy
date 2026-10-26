#include "ChunkMesher.h"
#include "ChunkStreamer.h"
#include "TerrainGenerator.h"
#include "World.h"

#include <iostream>
#include <string>

int main()
{
    const auto require = [](bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            return false;
        }
        return true;
    };

    const ChunkAddress negativeAddress =
        WorldCoordinates::toChunkAddress({-1, 7, -17});
    if (!require(negativeAddress.chunk.x == -1 &&
                     negativeAddress.local.x == 15 &&
                     negativeAddress.chunk.z == -2 &&
                     negativeAddress.local.z == 15,
                 "Negative block coordinates map to the wrong chunk."))
        return 1;

    World infiniteWorld(WorldType::Infinite);
    if (!require(infiniteWorld.setBlock({-1, 7, -17}, BlockId::Stone) &&
                     infiniteWorld.getBlock({-1, 7, -17}) == BlockId::Stone,
                 "Infinite worlds must store blocks at negative coordinates."))
        return 1;

    World finiteWorld(WorldType::Limited);
    if (!require(finiteWorld.setBlock({399, 10, 399}, BlockId::Grass) &&
                     !finiteWorld.setBlock({400, 10, 399}, BlockId::Grass) &&
                     finiteWorld.getBlock({400, 10, 399}) == BlockId::Air,
                 "Finite world bounds are not enforced."))
        return 1;

    World firstWorld(WorldType::Infinite);
    World secondWorld(WorldType::Infinite);
    TerrainGenerator generator(2468);
    generator.generateChunk(firstWorld, {-2, 3});
    generator.generateChunk(secondWorld, {-2, 3});
    for (int x = -32; x < -16; ++x)
    {
        for (int z = 48; z < 64; ++z)
        {
            for (int y = 0; y < 12; ++y)
            {
                if (!require(firstWorld.getBlock({x, y, z}) ==
                                 secondWorld.getBlock({x, y, z}),
                             "Terrain generation must be deterministic."))
                    return 1;
            }
        }
    }

    World meshWorld(WorldType::Infinite);
    meshWorld.setBlock({0, 10, 0}, BlockId::Grass);
    if (!require(ChunkMesher::buildMesh(meshWorld, {0, 0}).size() == 36,
                 "An isolated block should emit six visible faces."))
        return 1;

    meshWorld.setBlock({1, 10, 0}, BlockId::Dirt);
    if (!require(ChunkMesher::buildMesh(meshWorld, {0, 0}).size() == 60,
                 "Adjacent blocks should hide their shared face."))
        return 1;

    meshWorld.setBlock({1, 10, 0}, BlockId::Air);
    meshWorld.removeChunk({0, 0});
    generator.generateChunk(meshWorld, {0, 0});
    if (!require(meshWorld.getBlock({1, 10, 0}) == BlockId::Air,
                 "Player block edits must persist after chunk regeneration."))
        return 1;

    World seamWorld(WorldType::Infinite);
    seamWorld.setBlock({15, 10, 0}, BlockId::Stone);
    seamWorld.setBlock({16, 10, 0}, BlockId::Stone);
    if (!require(ChunkMesher::buildMesh(seamWorld, {0, 0}).size() == 30 &&
                     ChunkMesher::buildMesh(seamWorld, {1, 0}).size() == 30,
                 "Adjacent chunks must hide faces at their shared boundary."))
        return 1;

    World streamedWorld(WorldType::Infinite);
    TerrainGenerator streamGenerator(9876);
    ChunkStreamer::update(streamedWorld, streamGenerator, 0, 0, 1);
    if (!require(streamedWorld.getLoadedChunkCount() == 9,
                 "Infinite streaming should generate the requested chunk area."))
        return 1;
    streamedWorld.setBlock({0, 0, 0}, BlockId::Air);
    ChunkStreamer::update(streamedWorld, streamGenerator, 256, 0, 1);
    if (!require(streamedWorld.getLoadedChunkCount() == 9 &&
                     streamedWorld.getChunk({0, 0}) == nullptr &&
                     streamedWorld.getChunk({16, 0}) != nullptr,
                 "Infinite streaming should unload distant chunks."))
        return 1;
    ChunkStreamer::update(streamedWorld, streamGenerator, 0, 0, 1);
    if (!require(streamedWorld.getBlock({0, 0, 0}) == BlockId::Air,
                 "Block edits should persist when streamed chunks reload."))
        return 1;

    std::cout << "World tests passed.\n";
    return 0;
}
