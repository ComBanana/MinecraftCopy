
#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Player {
public:
    Player();

    void setPosition(const glm::vec3& newPosition);
    void processMouseMovement(double xpos, double ypos);
    void processMovement(GLFWwindow* window, float deltaTime,
                         const int heights[16][16]);

    glm::vec3 getPosition() const;
    glm::vec3 getFront() const;
    glm::vec3 getUp() const;

private:
    bool overlapsBlock(int x, int y, int z,
                       const int heights[16][16]) const;
    void moveHorizontal(float dx, float dz,
                        const int heights[16][16]);
    void updateVertical(float deltaTime,
                        const int heights[16][16]);

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
