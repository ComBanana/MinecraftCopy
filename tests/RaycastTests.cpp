#include "BlockRaycaster.h"

#include <iostream>

int main()
{
    World world(WorldType::Infinite);
    world.setBlock({3, 2, 0}, BlockId::Stone);
    world.setBlock({4, 2, 0}, BlockId::Grass);

    const BlockTarget positiveHit = raycastBlocks(
        world, {0.5f, 2.5f, 0.5f}, {1.0f, 0.0f, 0.0f});
    if (!positiveHit.hit || positiveHit.x != 3 || positiveHit.y != 2 ||
        positiveHit.z != 0 || positiveHit.previousBlock != glm::ivec3(2, 2, 0))
    {
        std::cerr << "Raycast did not return the first block and adjacent cell.\n";
        return 1;
    }

    world.setBlock({-2, 1, 0}, BlockId::Dirt);
    const BlockTarget negativeHit = raycastBlocks(
        world, {0.5f, 1.5f, 0.5f}, {-1.0f, 0.0f, 0.0f});
    if (!negativeHit.hit || negativeHit.x != -2 ||
        negativeHit.previousBlock != glm::ivec3(-1, 1, 0))
    {
        std::cerr << "Raycast failed across negative block coordinates.\n";
        return 1;
    }

    World reachWorld(WorldType::Infinite);
    reachWorld.setBlock({6, 2, 0}, BlockId::Stone);
    const BlockTarget outOfReach = raycastBlocks(
        reachWorld, {0.5f, 2.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 5.0f);
    if (outOfReach.hit)
    {
        std::cerr << "Raycast hit a block beyond maximum reach.\n";
        return 1;
    }

    const BlockTarget noDirection = raycastBlocks(
        world, {0.5f, 2.5f, 0.5f}, {0.0f, 0.0f, 0.0f});
    if (noDirection.hit)
    {
        std::cerr << "Raycast should miss for a zero direction.\n";
        return 1;
    }

    std::cout << "Raycast tests passed.\n";
    return 0;
}
