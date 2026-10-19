
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

const int WIDTH = 800;
const int HEIGHT = 600;

const int WORLD_X = 16;
const int WORLD_Z = 16;

const float PLAYER_HEIGHT = 1.8f;
const float PLAYER_RADIUS = 0.3f;
const float MOVE_SPEED = 5.0f;
const float GRAVITY = 20.0f;
const float JUMP_SPEED = 7.0f;

int heights[WORLD_X][WORLD_Z];

glm::vec3 cameraPos(8.0f, 3.8f, 8.0f);
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

float yaw = -90.0f;
float pitch = 0.0f;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float verticalVelocity = 0.0f;
bool onGround = false;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    const float sensitivity = 0.1f;
    yaw += xoffset * sensitivity;
    pitch += yoffset * sensitivity;

    pitch = std::clamp(pitch, -89.0f, 89.0f);

    glm::vec3 front;
    front.x = std::cos(glm::radians(yaw)) *
              std::cos(glm::radians(pitch));
    front.y = std::sin(glm::radians(pitch));
    front.z = std::sin(glm::radians(yaw)) *
              std::cos(glm::radians(pitch));

    cameraFront = glm::normalize(front);
}

void framebuffer_callback(GLFWwindow*, int width, int height) {
    if (height > 0 && width > 0) {
        glViewport(0, 0, width, height);
    }
}

unsigned int makeShaderProgram() {
    const char* vertexSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;

        uniform mat4 MVP;
        out vec3 vColor;

        void main() {
            vColor = aColor;
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentSource = R"(
        #version 330 core
        in vec3 vColor;
        out vec4 fragColor;

        void main() {
            fragColor = vec4(vColor, 1.0);
        }
    )";

    auto compileShader = [](unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader compilation failed:\n" << log << '\n';
            glDeleteShader(shader);
            return 0u;
        }

        return shader;
    };

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader linking failed:\n" << log << '\n';
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void addFace(std::vector<Vertex>& vertices,
             glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
             glm::vec3 color) {
    vertices.push_back({a, color});
    vertices.push_back({b, color});
    vertices.push_back({c, color});

    vertices.push_back({a, color});
    vertices.push_back({c, color});
    vertices.push_back({d, color});
}

std::vector<Vertex> buildWorld() {
    std::vector<Vertex> vertices;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            // Simple rolling terrain.
            heights[x][z] = 1 + (x + z) % 3;
        }
    }

    const glm::vec3 grass(0.15f, 0.72f, 0.16f);
    const glm::vec3 dirt(0.48f, 0.30f, 0.17f);
    const glm::vec3 stone(0.48f, 0.49f, 0.51f);

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            int h = heights[x][z];

            for (int y = 0; y < h; ++y) {
                glm::vec3 p(x, y, z);
                glm::vec3 topColor = (y == h - 1) ? grass : dirt;

                // Top
                addFace(vertices,
                    p + glm::vec3(0, 1, 0),
                    p + glm::vec3(0, 1, 1),
                    p + glm::vec3(1, 1, 1),
                    p + glm::vec3(1, 1, 0),
                    topColor);

                // Bottom
                addFace(vertices,
                    p + glm::vec3(0, 0, 0),
                    p + glm::vec3(1, 0, 0),
                    p + glm::vec3(1, 0, 1),
                    p + glm::vec3(0, 0, 1),
                    dirt);

                // +X
                addFace(vertices,
                    p + glm::vec3(1, 0, 0),
                    p + glm::vec3(1, 1, 0),
                    p + glm::vec3(1, 1, 1),
                    p + glm::vec3(1, 0, 1),
                    stone);

                // -X
                addFace(vertices,
                    p + glm::vec3(0, 0, 0),
                    p + glm::vec3(0, 0, 1),
                    p + glm::vec3(0, 1, 1),
                    p + glm::vec3(0, 1, 0),
                    stone);

                // +Z
                addFace(vertices,
                    p + glm::vec3(0, 0, 1),
                    p + glm::vec3(1, 0, 1),
                    p + glm::vec3(1, 1, 1),
                    p + glm::vec3(0, 1, 1),
                    stone);

                // -Z
                addFace(vertices,
                    p + glm::vec3(0, 0, 0),
                    p + glm::vec3(0, 1, 0),
                    p + glm::vec3(1, 1, 0),
                    p + glm::vec3(1, 0, 0),
                    stone);
            }
        }
    }

    return vertices;
}

// Checks overlap between the player's body and one solid voxel.
bool overlapsBlock(int x, int y, int z) {
    float playerMinX = cameraPos.x - PLAYER_RADIUS;
    float playerMaxX = cameraPos.x + PLAYER_RADIUS;
    float playerMinY = cameraPos.y - PLAYER_HEIGHT;
    float playerMaxY = cameraPos.y;
    float playerMinZ = cameraPos.z - PLAYER_RADIUS;
    float playerMaxZ = cameraPos.z + PLAYER_RADIUS;

    return playerMaxX > x && playerMinX < x + 1 &&
           playerMaxY > y && playerMinY < y + 1 &&
           playerMaxZ > z && playerMinZ < z + 1;
}

void movePlayer(float dx, float dz) {
    // Resolve X movement independently.
    float oldX = cameraPos.x;
    cameraPos.x += dx;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < heights[x][z]; ++y) {
                if (overlapsBlock(x, y, z)) {
                    if (dx > 0.0f)
                        cameraPos.x = x - PLAYER_RADIUS;
                    else if (dx < 0.0f)
                        cameraPos.x = x + 1.0f + PLAYER_RADIUS;
                }
            }
        }
    }

    // Resolve Z movement independently.
    float oldZ = cameraPos.z;
    cameraPos.z += dz;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < heights[x][z]; ++y) {
                if (overlapsBlock(x, y, z)) {
                    if (dz > 0.0f)
                        cameraPos.z = z - PLAYER_RADIUS;
                    else if (dz < 0.0f)
                        cameraPos.z = z + 1.0f + PLAYER_RADIUS;
                }
            }
        }
    }

    (void)oldX;
    (void)oldZ;
}

void updateVertical(float dt) {
    onGround = false;

    float oldEyeY = cameraPos.y;
    float oldFeetY = oldEyeY - PLAYER_HEIGHT;

    verticalVelocity -= GRAVITY * dt;
    cameraPos.y += verticalVelocity * dt;

    for (int x = 0; x < WORLD_X; ++x) {
        for (int z = 0; z < WORLD_Z; ++z) {
            for (int y = 0; y < heights[x][z]; ++y) {
                if (!overlapsBlock(x, y, z))
                    continue;

                float blockTop = static_cast<float>(y + 1);
                float blockBottom = static_cast<float>(y);

                if (verticalVelocity <= 0.0f &&
                    oldFeetY >= blockTop - 0.001f) {
                    // Land on the top of the block.
                    cameraPos.y = blockTop + PLAYER_HEIGHT;
                    verticalVelocity = 0.0f;
                    onGround = true;
                } else if (verticalVelocity > 0.0f &&
                           oldEyeY <= blockBottom + 0.001f) {
                    // Hit the underside of a block.
                    cameraPos.y = blockBottom;
                    verticalVelocity = 0.0f;
                }
            }
        }
    }

    // A small downward probe keeps the grounded state stable.
    if (!onGround && verticalVelocity <= 0.0f) {
        float savedY = cameraPos.y;
        cameraPos.y -= 0.03f;

        for (int x = 0; x < WORLD_X && !onGround; ++x) {
            for (int z = 0; z < WORLD_Z && !onGround; ++z) {
                for (int y = 0; y < heights[x][z]; ++y) {
                    if (overlapsBlock(x, y, z)) {
                        cameraPos.y = static_cast<float>(y + 1) +
                                      PLAYER_HEIGHT;
                        verticalVelocity = 0.0f;
                        onGround = true;
                        break;
                    }
                }
            }
        }

        if (!onGround)
            cameraPos.y = savedY;
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        WIDTH, HEIGHT, "MinecraftCopy", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Correct initialization for GLAD 2.
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Build the world before positioning the player.
    std::vector<Vertex> vertices = buildWorld();

    // Spawn above the ground at the center of the world.
    cameraPos = glm::vec3(
        8.5f,
        static_cast<float>(heights[8][8]) + PLAYER_HEIGHT,
        8.5f);

    unsigned int shaderProgram = makeShaderProgram();
    if (!shaderProgram) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    unsigned int VAO = 0;
    unsigned int VBO = 0;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);

    bool spaceWasDown = false;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = std::min(currentFrame - lastFrame, 0.05f);
        lastFrame = currentFrame;

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // WASD movement relative to the direction the camera faces.
        glm::vec3 forward(
            cameraFront.x, 0.0f, cameraFront.z);

        if (glm::length(forward) > 0.001f)
            forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(
            glm::cross(forward, cameraUp));

        glm::vec3 moveDirection(0.0f);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            moveDirection += forward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            moveDirection -= forward;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            moveDirection += right;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            moveDirection -= right;

        if (glm::length(moveDirection) > 0.001f) {
            moveDirection = glm::normalize(moveDirection);
            movePlayer(
                moveDirection.x * MOVE_SPEED * deltaTime,
                moveDirection.z * MOVE_SPEED * deltaTime);
        }

        // Jump only on a fresh press while grounded.
        bool spaceDown =
            glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

        if (spaceDown && !spaceWasDown && onGround) {
            verticalVelocity = JUMP_SPEED;
            onGround = false;
        }

        spaceWasDown = spaceDown;

        updateVertical(deltaTime);

        // Camera and projection.
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);

        if (fbh == 0) {
            glfwPollEvents();
            continue;
        }

        glViewport(0, 0, fbw, fbh);
        glClearColor(0.53f, 0.80f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(70.0f),
            static_cast<float>(fbw) / static_cast<float>(fbh),
            0.1f, 100.0f);

        glm::mat4 view = glm::lookAt(
            cameraPos, cameraPos + cameraFront, cameraUp);

        glm::mat4 MVP = projection * view;

        glUseProgram(shaderProgram);
        GLint mvpLocation =
            glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(
            mvpLocation, 1, GL_FALSE, &MVP[0][0]);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0,
                     static_cast<GLsizei>(vertices.size()));

        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
