
#include "Player.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr float PLAYER_HEIGHT = 1.8f;
    constexpr float PLAYER_RADIUS = 0.3f;
    constexpr float MOVE_SPEED = 5.0f;
    constexpr float GRAVITY = 20.0f;
    constexpr float JUMP_SPEED = 7.0f;
    constexpr float MOUSE_SENSITIVITY = 0.1f;
}

Player::Player()
    : position(8.5f, 4.8f, 8.5f),
      front(0.0f, 0.0f, -1.0f),
      up(0.0f, 1.0f, 0.0f),
      yaw(-90.0f),
      pitch(0.0f),
      verticalVelocity(0.0f),
      onGround(false),
      firstMouse(true),
      spaceWasDown(false),
      lastX(400.0f),
      lastY(300.0f) {}

void Player::setPosition(const glm::vec3& newPosition) {
    position = newPosition;
    verticalVelocity = 0.0f;
    onGround = false;
}

void Player::resetMouseTracking() {
    firstMouse = true;
}

void Player::processMouseMovement(double xpos, double ypos) {
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    const float xOffset = (static_cast<float>(xpos) - lastX)
                          * MOUSE_SENSITIVITY;
    const float yOffset = (lastY - static_cast<float>(ypos))
                          * MOUSE_SENSITIVITY;

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    yaw += xOffset;
    pitch = std::clamp(pitch + yOffset, -89.0f, 89.0f);

    glm::vec3 direction;
    direction.x = std::cos(glm::radians(yaw))
                  * std::cos(glm::radians(pitch));
    direction.y = std::sin(glm::radians(pitch));
    direction.z = std::sin(glm::radians(yaw))
                  * std::cos(glm::radians(pitch));

    front = glm::normalize(direction);
}

bool Player::overlapsBlock(
    int x, int y, int z, const World& world) const {

    if (world.getBlock({x, y, z}) == BlockId::Air)
        return false;

    float minX = position.x - PLAYER_RADIUS;
    float maxX = position.x + PLAYER_RADIUS;
    float minY = position.y - PLAYER_HEIGHT;
    float maxY = position.y;
    float minZ = position.z - PLAYER_RADIUS;
    float maxZ = position.z + PLAYER_RADIUS;

    return maxX > x && minX < x + 1 &&
           maxY > y && minY < y + 1 &&
           maxZ > z && minZ < z + 1;
}


void Player::moveHorizontal(
    float dx, float dz, const World& world) {

    // Resolve X movement first, stopping at the nearest block.
    const float oldX = position.x;
    position.x += dx;
    if (world.getType() == WorldType::Limited)
    {
        position.x = std::clamp(
            position.x,
            PLAYER_RADIUS,
            static_cast<float>(WorldConfig::LIMITED_BLOCKS_X) - PLAYER_RADIUS);
    }
    float resolvedX = position.x;

    {
        const int minX = static_cast<int>(std::floor(position.x - PLAYER_RADIUS));
        const int maxX = static_cast<int>(std::floor(position.x + PLAYER_RADIUS));
        const int minY = static_cast<int>(std::floor(position.y - PLAYER_HEIGHT));
        const int maxY = static_cast<int>(std::floor(position.y));
        const int minZ = static_cast<int>(std::floor(position.z - PLAYER_RADIUS));
        const int maxZ = static_cast<int>(std::floor(position.z + PLAYER_RADIUS));
        for (int x = minX; x <= maxX; ++x) {
            for (int z = minZ; z <= maxZ; ++z) {
                for (int y = minY; y <= maxY; ++y) {
                if (!overlapsBlock(x, y, z, world))
                    continue;

                if (dx > 0.0f)
                    resolvedX = std::min(
                        resolvedX, x - PLAYER_RADIUS);
                else if (dx < 0.0f)
                    resolvedX = std::max(
                        resolvedX, x + 1.0f + PLAYER_RADIUS);
                }
            }
        }
    }

    position.x = (dx == 0.0f) ? oldX : resolvedX;

    // Resolve Z movement using the corrected X position.
    const float oldZ = position.z;
    position.z += dz;
    if (world.getType() == WorldType::Limited)
    {
        position.z = std::clamp(
            position.z,
            PLAYER_RADIUS,
            static_cast<float>(WorldConfig::LIMITED_BLOCKS_Z) - PLAYER_RADIUS);
    }
    float resolvedZ = position.z;

    {
        const int minX = static_cast<int>(std::floor(position.x - PLAYER_RADIUS));
        const int maxX = static_cast<int>(std::floor(position.x + PLAYER_RADIUS));
        const int minY = static_cast<int>(std::floor(position.y - PLAYER_HEIGHT));
        const int maxY = static_cast<int>(std::floor(position.y));
        const int minZ = static_cast<int>(std::floor(position.z - PLAYER_RADIUS));
        const int maxZ = static_cast<int>(std::floor(position.z + PLAYER_RADIUS));
        for (int x = minX; x <= maxX; ++x) {
            for (int z = minZ; z <= maxZ; ++z) {
                for (int y = minY; y <= maxY; ++y) {
                if (!overlapsBlock(x, y, z, world))
                    continue;

                if (dz > 0.0f)
                    resolvedZ = std::min(
                        resolvedZ, z - PLAYER_RADIUS);
                else if (dz < 0.0f)
                    resolvedZ = std::max(
                        resolvedZ, z + 1.0f + PLAYER_RADIUS);
                }
            }
        }
    }

    position.z = (dz == 0.0f) ? oldZ : resolvedZ;
}


void Player::updateVertical(
    float dt, const World& world) {

    onGround = false;

    float oldEyeY = position.y;
    float oldFeetY = oldEyeY - PLAYER_HEIGHT;

    verticalVelocity -= GRAVITY * dt;
    position.y += verticalVelocity * dt;

    {
        const int minX = static_cast<int>(std::floor(position.x - PLAYER_RADIUS));
        const int maxX = static_cast<int>(std::floor(position.x + PLAYER_RADIUS));
        const int minY = static_cast<int>(std::floor(position.y - PLAYER_HEIGHT));
        const int maxY = static_cast<int>(std::floor(position.y));
        const int minZ = static_cast<int>(std::floor(position.z - PLAYER_RADIUS));
        const int maxZ = static_cast<int>(std::floor(position.z + PLAYER_RADIUS));
        for (int x = minX; x <= maxX; ++x) {
            for (int z = minZ; z <= maxZ; ++z) {
                for (int y = minY; y <= maxY; ++y) {
                if (!overlapsBlock(x, y, z, world))
                    continue;

                float blockTop = static_cast<float>(y + 1);
                float blockBottom = static_cast<float>(y);

                if (verticalVelocity <= 0.0f &&
                    oldFeetY >= blockTop - 0.001f) {
                    position.y = blockTop + PLAYER_HEIGHT;
                    verticalVelocity = 0.0f;
                    onGround = true;
                } else if (verticalVelocity > 0.0f &&
                           oldEyeY <= blockBottom + 0.001f) {
                    position.y = blockBottom;
                    verticalVelocity = 0.0f;
                }
                }
            }
        }
    }

    if (!onGround && verticalVelocity <= 0.0f) {
        float savedY = position.y;
        position.y -= 0.03f;

        const int minX = static_cast<int>(std::floor(position.x - PLAYER_RADIUS));
        const int maxX = static_cast<int>(std::floor(position.x + PLAYER_RADIUS));
        const int minY = static_cast<int>(std::floor(position.y - PLAYER_HEIGHT));
        const int maxY = static_cast<int>(std::floor(position.y));
        const int minZ = static_cast<int>(std::floor(position.z - PLAYER_RADIUS));
        const int maxZ = static_cast<int>(std::floor(position.z + PLAYER_RADIUS));
        for (int x = minX; x <= maxX && !onGround; ++x) {
            for (int z = minZ; z <= maxZ && !onGround; ++z) {
                for (int y = minY; y <= maxY; ++y) {
                    if (overlapsBlock(x, y, z, world)) {
                        position.y = static_cast<float>(y + 1)
                                     + PLAYER_HEIGHT;
                        verticalVelocity = 0.0f;
                        onGround = true;
                        break;
                    }
                }
            }
        }

        if (!onGround)
            position.y = savedY;
    }
}

void Player::processMovement(
    GLFWwindow* window, float dt, const World& world) {

    const PlayerInput input{
        glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
        glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS};
    updateMovement(dt, world, input);
}

void Player::updateMovement(
    float dt, const World& world, const PlayerInput& input) {

    glm::vec3 forward(front.x, 0.0f, front.z);

    if (glm::length(forward) > 0.001f)
        forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 direction(0.0f);

    if (input.forward)
        direction += forward;
    if (input.backward)
        direction -= forward;
    if (input.right)
        direction += right;
    if (input.left)
        direction -= right;

    if (glm::length(direction) > 0.001f) {
        direction = glm::normalize(direction);
        moveHorizontal(direction.x * MOVE_SPEED * dt,
                       direction.z * MOVE_SPEED * dt, world);
    }

    if (input.jump && !spaceWasDown && onGround) {
        verticalVelocity = JUMP_SPEED;
        onGround = false;
    }

    spaceWasDown = input.jump;
    updateVertical(dt, world);
}

glm::vec3 Player::getPosition() const { return position; }
glm::vec3 Player::getFront() const { return front; }
glm::vec3 Player::getUp() const { return up; }
