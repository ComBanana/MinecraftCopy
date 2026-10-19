
#include "Player.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr int WORLD_X = 16;
    constexpr int WORLD_Z = 16;

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

void Player::processMouseMovement(double xpos, double ypos) {
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xOffset = (static_cast<float>(xpos) - lastX)
                    * MOUSE_SENSITIVITY;
    float yOffset = (lastY - static_cast<float>(ypos))
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
    int x, int y, int z, const int heights[16][16]) const {

    if (x < 0 || x >= WORLD_X || z < 0 || z >= WORLD_Z)
        return false;
    if (y < 0 || y >= heights[x][z])
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
    float dx, float dz, const int heights[16][16]) {

    // Resolve X movement first, stopping at the nearest block.
    const float oldX = position.x;
    position.x += dx;
    float resolvedX = position.x;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < heights[x][z]; ++y) {
                if (!overlapsBlock(x, y, z, heights))
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

    position.x = (dx == 0.0f) ? oldX : resolvedX;

    // Resolve Z movement using the corrected X position.
    const float oldZ = position.z;
    position.z += dz;
    float resolvedZ = position.z;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < heights[x][z]; ++y) {
                if (!overlapsBlock(x, y, z, heights))
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

    position.z = (dz == 0.0f) ? oldZ : resolvedZ;
}


void Player::updateVertical(
    float dt, const int heights[16][16]) {

    onGround = false;

    float oldEyeY = position.y;
    float oldFeetY = oldEyeY - PLAYER_HEIGHT;

    verticalVelocity -= GRAVITY * dt;
    position.y += verticalVelocity * dt;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < heights[x][z]; ++y) {
                if (!overlapsBlock(x, y, z, heights))
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

    if (!onGround && verticalVelocity <= 0.0f) {
        float savedY = position.y;
        position.y -= 0.03f;

        for (int x = 0; x < WORLD_X && !onGround; ++x) {
            for (int z = 0; z < WORLD_Z && !onGround; ++z) {
                for (int y = 0; y < heights[x][z]; ++y) {
                    if (overlapsBlock(x, y, z, heights)) {
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
    GLFWwindow* window, float dt, const int heights[16][16]) {

    glm::vec3 forward(front.x, 0.0f, front.z);

    if (glm::length(forward) > 0.001f)
        forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 direction(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        direction += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        direction -= forward;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        direction += right;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        direction -= right;

    if (glm::length(direction) > 0.001f) {
        direction = glm::normalize(direction);
        moveHorizontal(direction.x * MOVE_SPEED * dt,
                       direction.z * MOVE_SPEED * dt, heights);
    }

    bool spaceDown =
        glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

    if (spaceDown && !spaceWasDown && onGround) {
        verticalVelocity = JUMP_SPEED;
        onGround = false;
    }

    spaceWasDown = spaceDown;
    updateVertical(dt, heights);
}

glm::vec3 Player::getPosition() const { return position; }
glm::vec3 Player::getFront() const { return front; }
glm::vec3 Player::getUp() const { return up; }
