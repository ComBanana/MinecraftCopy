
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "Player.h"

const int WIDTH = 800;
const int HEIGHT = 600;

const int WORLD_X = 16;
const int WORLD_Z = 16;

int heights[WORLD_X][WORLD_Z];

float deltaTime = 0.0f;
float lastFrame = 0.0f;
Player player;

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
};

void mouse_callback(GLFWwindow *, double xpos, double ypos)
{
    player.processMouseMovement(xpos, ypos);
}

void framebuffer_callback(GLFWwindow *, int width, int height)
{
    if (height > 0 && width > 0)
    {
        glViewport(0, 0, width, height);
    }
}

unsigned int makeShaderProgram()
{
    const char *vertexSource = R"(
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

    const char *fragmentSource = R"(
        #version 330 core
        in vec3 vColor;
        out vec4 fragColor;

        void main() {
            fragColor = vec4(vColor, 1.0);
        }
    )";

    auto compileShader = [](unsigned int type, const char *source)
    {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success)
        {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader compilation failed:\n"
                      << log << '\n';
            glDeleteShader(shader);
            return 0u;
        }

        return shader;
    };

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (!vs || !fs)
    {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
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

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader linking failed:\n"
                  << log << '\n';
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void addFace(std::vector<Vertex> &vertices,
             glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
             glm::vec3 color)
{
    vertices.push_back({a, color});
    vertices.push_back({b, color});
    vertices.push_back({c, color});

    vertices.push_back({a, color});
    vertices.push_back({c, color});
    vertices.push_back({d, color});
}

std::vector<Vertex> buildWorld()
{
    std::vector<Vertex> vertices;

    const glm::vec3 grass(0.15f, 0.72f, 0.16f);
    const glm::vec3 dirt(0.48f, 0.30f, 0.17f);
    const glm::vec3 stone(0.48f, 0.49f, 0.51f);

    for (int x = 0; x < WORLD_X; ++x)
    {
        for (int z = 0; z < WORLD_Z; ++z)
        {
            int h = heights[x][z];

            for (int y = 0; y < h; ++y)
            {
                glm::vec3 p(x, y, z);
                glm::vec3 topColor = (y == h - 1) ? grass : dirt;

                // Top: only the highest block in this column has an exposed top.
                if (y == h - 1)
                {
                    addFace(vertices,
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 1, 0),
                            topColor);
                }

                // Bottom: only the bottom layer touches the outside world.
                if (y == 0)
                {
                    addFace(vertices,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(1, 0, 0),
                            p + glm::vec3(1, 0, 1),
                            p + glm::vec3(0, 0, 1),
                            dirt);
                }

                // +X: draw only if the neighboring column doesn't cover this block.
                if (x == WORLD_X - 1 || heights[x + 1][z] <= y)
                {
                    addFace(vertices,
                            p + glm::vec3(1, 0, 0),
                            p + glm::vec3(1, 1, 0),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 0, 1),
                            stone);
                }

                // -X
                if (x == 0 || heights[x - 1][z] <= y)
                {
                    addFace(vertices,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(0, 0, 1),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(0, 1, 0),
                            stone);
                }

                // +Z
                if (z == WORLD_Z - 1 || heights[x][z + 1] <= y)
                {
                    addFace(vertices,
                            p + glm::vec3(0, 0, 1),
                            p + glm::vec3(1, 0, 1),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(0, 1, 1),
                            stone);
                }

                // -Z
                if (z == 0 || heights[x][z - 1] <= y)
                {
                    addFace(vertices,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(1, 1, 0),
                            p + glm::vec3(1, 0, 0),
                            stone);
                }
            }
        }
    }

    return vertices;
}

struct BlockTarget
{
    bool hit = false;
    int x = 0;
    int y = 0;
    int z = 0;
    glm::ivec3 previousBlock = glm::ivec3(0);
};

BlockTarget raycastBlocks(
    const glm::vec3 &origin,
    const glm::vec3 &direction,
    const int heights[16][16])
{
    constexpr float MAX_REACH = 5.0f;
    constexpr float STEP = 0.02f;

    BlockTarget result;
    glm::ivec3 previous(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z)));

    for (float distance = 0.0f;
         distance <= MAX_REACH;
         distance += STEP)
    {
        glm::vec3 point = origin + direction * distance;

        int x = static_cast<int>(std::floor(point.x));
        int y = static_cast<int>(std::floor(point.y));
        int z = static_cast<int>(std::floor(point.z));

        if (x < 0 || x >= 16 || z < 0 || z >= 16)
            continue;

        if (y >= 0 && y < heights[x][z])
        {
            result.hit = true;
            result.x = x;
            result.y = y;
            result.z = z;
            result.previousBlock = previous;
            return result;
        }

        previous = glm::ivec3(x, y, z);
    }

    return result;
}

void addTargetOutline(std::vector<Vertex> &vertices,
                      const BlockTarget &target)
{
    if (!target.hit)
        return;

    const float e = 0.003f;
    float x = static_cast<float>(target.x);
    float y = static_cast<float>(target.y);
    float z = static_cast<float>(target.z);

    glm::vec3 p[8] = {
        {x - e, y - e, z - e}, {x + 1 + e, y - e, z - e}, {x + 1 + e, y + 1 + e, z - e}, {x - e, y + 1 + e, z - e}, {x - e, y - e, z + 1 + e}, {x + 1 + e, y - e, z + 1 + e}, {x + 1 + e, y + 1 + e, z + 1 + e}, {x - e, y + 1 + e, z + 1 + e}};

    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    glm::vec3 color(0.05f, 0.05f, 0.05f);

    for (const auto &edge : edges)
    {
        vertices.push_back({p[edge[0]], color});
        vertices.push_back({p[edge[1]], color});
    }
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(
        WIDTH, HEIGHT, "MinecraftCopy", nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    // Correct initialization for GLAD 2.
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)))
    {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Generate the initial terrain only once.
    for (int x = 0; x < WORLD_X; ++x)
    {
        for (int z = 0; z < WORLD_Z; ++z)
        {
            heights[x][z] = 1 + (x + z) % 3;
        }
    }

    std::vector<Vertex> vertices = buildWorld();

    // Spawn above the ground at the center of the world.
    player.setPosition(glm::vec3(
        8.5f,
        static_cast<float>(heights[8][8]) + 1.8f,
        8.5f));

    unsigned int shaderProgram = makeShaderProgram();
    if (!shaderProgram)
    {
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
        reinterpret_cast<void *>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);

    // Buffers for the block-selection outline.
    unsigned int outlineVAO = 0;
    unsigned int outlineVBO = 0;

    glGenVertexArrays(1, &outlineVAO);
    glGenBuffers(1, &outlineVBO);

    glBindVertexArray(outlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);

    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(Vertex),
                 nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);

    // Restore the terrain VAO.
    glBindVertexArray(VAO);

    bool leftMouseWasDown = false;
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = std::min(currentFrame - lastFrame, 0.05f);
        lastFrame = currentFrame;

        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, true);
        }

        player.processMovement(window, deltaTime, heights);

        BlockTarget target = raycastBlocks(
            player.getPosition(),
            player.getFront(),
            heights);

        // Break the targeted top block on a fresh left-click.
        bool leftMouseDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        if (leftMouseDown && !leftMouseWasDown && target.hit)
        {
            if (target.y == heights[target.x][target.z] - 1)
            {
                heights[target.x][target.z]--;

                // Rebuild the terrain mesh after removing the block.
                vertices = buildWorld();

                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(
                        vertices.size() * sizeof(Vertex)),
                    vertices.data(),
                    GL_DYNAMIC_DRAW);

                glBindVertexArray(VAO);
            }
        }

        leftMouseWasDown = leftMouseDown;

        bool rightMouseWasDown = false;
        bool rightMouseDown =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        if (rightMouseDown && !rightMouseWasDown && target.hit)
        {
            glm::ivec3 place = target.previousBlock;

            if (place.x >= 0 && place.x < WORLD_X &&
                place.z >= 0 && place.z < WORLD_Z &&
                place.y >= 0 &&
                place.y == heights[place.x][place.z])
            {
                glm::vec3 p = player.getPosition();

                bool overlaps =
                    p.x + 0.3f > place.x && p.x - 0.3f < place.x + 1 &&
                    p.y > place.y && p.y - 1.8f < place.y + 1 &&
                    p.z + 0.3f > place.z && p.z - 0.3f < place.z + 1;

                if (!overlaps)
                {
                    heights[place.x][place.z]++;
                    vertices = buildWorld();

                    glBindBuffer(GL_ARRAY_BUFFER, VBO);
                    glBufferData(
                        GL_ARRAY_BUFFER,
                        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                        vertices.data(),
                        GL_DYNAMIC_DRAW);
                    glBindVertexArray(VAO);
                }
            }
        }

        rightMouseWasDown = rightMouseDown;

        // Camera and projection.
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);

        if (fbh == 0)
        {
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
            player.getPosition(),
            player.getPosition() + player.getFront(),
            player.getUp());

        glm::mat4 MVP = projection * view;

        glUseProgram(shaderProgram);
        GLint mvpLocation =
            glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(
            mvpLocation, 1, GL_FALSE, &MVP[0][0]);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0,
                     static_cast<GLsizei>(vertices.size()));

        if (target.hit)
        {
            std::vector<Vertex> outlineVertices;
            outlineVertices.reserve(24);
            addTargetOutline(outlineVertices, target);

            glBindVertexArray(outlineVAO);
            glBindBuffer(GL_ARRAY_BUFFER, outlineVBO);

            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    outlineVertices.size() * sizeof(Vertex)),
                outlineVertices.data(),
                GL_DYNAMIC_DRAW);

            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0,
                         static_cast<GLsizei>(outlineVertices.size()));

            glBindVertexArray(VAO);
        }

        // Draw a white crosshair in the center of the screen.
        glEnable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // Horizontal bar
        glScissor(fbw / 2 - 8, fbh / 2 - 1, 17, 3);
        glClear(GL_COLOR_BUFFER_BIT);

        // Vertical bar
        glScissor(fbw / 2 - 1, fbh / 2 - 8, 3, 17);
        glClear(GL_COLOR_BUFFER_BIT);

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
