
#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "World.h"

struct PlayerInput
{
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool jump = false;
};

class Player {
public:
    Player();

    void setPosition(const glm::vec3& newPosition);
    void resetMouseTracking();
    void processMouseMovement(double xpos, double ypos);
    void processMovement(GLFWwindow* window, float deltaTime,
                         const World& world);
    void updateMovement(float deltaTime, const World& world,
                        const PlayerInput& input);

    glm::vec3 getPosition() const;
    glm::vec3 getFront() const;
    glm::vec3 getUp() const;

private:
    bool overlapsBlock(int x, int y, int z,
                       const World& world) const;
    void moveHorizontal(float dx, float dz, const World& world);
    void updateVertical(float deltaTime, const World& world);

    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw;
    float pitch;
    float verticalVelocity;

    bool onGround;
    bool firstMouse;
    bool spaceWasDown;

    float lastX;
    float lastY;
};
