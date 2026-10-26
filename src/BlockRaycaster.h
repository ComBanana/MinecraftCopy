#pragma once

#include "World.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

struct BlockTarget
{
    bool hit = false;
    int x = 0;
    int y = 0;
    int z = 0;
    glm::ivec3 previousBlock = glm::ivec3(0);
};

inline BlockTarget raycastBlocks(
    const World& world,
    const glm::vec3& origin,
    const glm::vec3& rayDirection,
    float maxReach = 5.0f)
{
    BlockTarget result;
    const float directionLength = glm::length(rayDirection);
    if (directionLength <= 0.000001f || maxReach < 0.0f)
        return result;

    const glm::vec3 direction = rayDirection / directionLength;
    glm::ivec3 cell(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z)));
    glm::ivec3 previous = cell;

    const auto recordHit = [&result, &previous](const glm::ivec3& block)
    {
        result.hit = true;
        result.x = block.x;
        result.y = block.y;
        result.z = block.z;
        result.previousBlock = previous;
    };

    if (world.getBlock({cell.x, cell.y, cell.z}) != BlockId::Air)
    {
        recordHit(cell);
        return result;
    }

    const glm::ivec3 step(
        direction.x > 0.0f ? 1 : direction.x < 0.0f ? -1 : 0,
        direction.y > 0.0f ? 1 : direction.y < 0.0f ? -1 : 0,
        direction.z > 0.0f ? 1 : direction.z < 0.0f ? -1 : 0);
    const float infinity = std::numeric_limits<float>::infinity();
    const glm::vec3 delta(
        step.x == 0 ? infinity : std::abs(1.0f / direction.x),
        step.y == 0 ? infinity : std::abs(1.0f / direction.y),
        step.z == 0 ? infinity : std::abs(1.0f / direction.z));
    glm::vec3 nextBoundary(
        step.x > 0 ? (static_cast<float>(cell.x + 1) - origin.x) / direction.x
                   : step.x < 0 ? (origin.x - static_cast<float>(cell.x)) / -direction.x : infinity,
        step.y > 0 ? (static_cast<float>(cell.y + 1) - origin.y) / direction.y
                   : step.y < 0 ? (origin.y - static_cast<float>(cell.y)) / -direction.y : infinity,
        step.z > 0 ? (static_cast<float>(cell.z + 1) - origin.z) / direction.z
                   : step.z < 0 ? (origin.z - static_cast<float>(cell.z)) / -direction.z : infinity);

    float distance = 0.0f;
    while (distance <= maxReach)
    {
        distance = std::min(nextBoundary.x,
                            std::min(nextBoundary.y, nextBoundary.z));
        if (distance > maxReach)
            break;

        previous = cell;
        const float tieTolerance = 0.000001f;
        if (nextBoundary.x <= distance + tieTolerance)
        {
            cell.x += step.x;
            nextBoundary.x += delta.x;
        }
        if (nextBoundary.y <= distance + tieTolerance)
        {
            cell.y += step.y;
            nextBoundary.y += delta.y;
        }
        if (nextBoundary.z <= distance + tieTolerance)
        {
            cell.z += step.z;
            nextBoundary.z += delta.z;
        }

        if (world.getBlock({cell.x, cell.y, cell.z}) != BlockId::Air)
        {
            recordHit(cell);
            return result;
        }
    }

    return result;
}
