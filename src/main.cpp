#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Player.h"
#include "BlockRaycaster.h"
#include "ChunkStreamer.h"
#include "TerrainGenerator.h"
#include "ChunkMesher.h"
#include "BuildVersion.h"

const int WIDTH = 800;
const int HEIGHT = 600;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
Player player;
World world(WorldType::Limited);
TerrainGenerator terrainGenerator;
bool infiniteWorld = false;
bool cursorLockRequested = true;
bool windowFocused = false;
bool suppressGameplayForResumeFrame = false;
using Vertex = ChunkVertex;

struct ChunkRenderMesh
{
    unsigned int vao = 0;
    unsigned int vbo = 0;
    GLsizei vertexCount = 0;
};

std::unordered_map<ChunkPos, ChunkRenderMesh, ChunkPosHash> chunkRenderMeshes;

void setCursorLock(GLFWwindow *window, bool locked)
{
    cursorLockRequested = locked;

    if (locked)
    {
        glfwFocusWindow(window);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwSetCursorPos(window, WIDTH * 0.5, HEIGHT * 0.5);
        player.resetMouseTracking();
        suppressGameplayForResumeFrame = true;
    }
    else
    {
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        player.resetMouseTracking();
    }
}

void mouse_callback(GLFWwindow *, double xpos, double ypos)
{
    if (cursorLockRequested && windowFocused)
        player.processMouseMovement(xpos, ypos);
}

void key_callback(GLFWwindow *window, int key, int, int action, int)
{
    if ((key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) &&
        action == GLFW_PRESS)
    {
        setCursorLock(window, !cursorLockRequested);
    }
}

void framebuffer_callback(GLFWwindow *, int width, int height)
{
    if (height > 0 && width > 0)
    {
        glViewport(0, 0, width, height);
    }
}

void focus_callback(GLFWwindow *window, int focused)
{
    windowFocused = focused == GLFW_TRUE;
    if (windowFocused && cursorLockRequested)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        glfwSetCursorPos(window, WIDTH * 0.5, HEIGHT * 0.5);
        player.resetMouseTracking();
        suppressGameplayForResumeFrame = true;
    }
}

unsigned int makeShaderProgram()
{
    const char *vertexSource = R"(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aColor;
    layout(location = 2) in vec2 aUV;

    uniform mat4 MVP;

    out vec3 vColor;
    out vec2 vUV;

    void main()
    {
        vColor = aColor;
        vUV = aUV;
        gl_Position = MVP * vec4(aPos, 1.0);
    }
)";

    const char *fragmentSource = R"(
    #version 330 core

    in vec3 vColor;
    in vec2 vUV;

    uniform sampler2D textureAtlas;
    uniform bool useTexture;

    out vec4 fragColor;

    void main()
    {
        if (useTexture)
            fragColor = texture(textureAtlas, vUV) * vec4(vColor, 1.0);
        else
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

void updateChunkMesh(const ChunkPos& chunkPos)
{
    const auto existing = chunkRenderMeshes.find(chunkPos);
    if (!world.getChunk(chunkPos))
    {
        if (existing != chunkRenderMeshes.end())
        {
            glDeleteVertexArrays(1, &existing->second.vao);
            glDeleteBuffers(1, &existing->second.vbo);
            chunkRenderMeshes.erase(existing);
        }
        return;
    }

    const std::vector<Vertex> vertices = ChunkMesher::buildMesh(world, chunkPos);
    ChunkRenderMesh& mesh = chunkRenderMeshes[chunkPos];
    if (mesh.vao == 0)
    {
        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, pos)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, color)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            reinterpret_cast<void*>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(2);
    }
    else
    {
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    }

    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(), GL_DYNAMIC_DRAW);
    mesh.vertexCount = static_cast<GLsizei>(vertices.size());
}

void syncChunkMeshes()
{
    std::unordered_set<ChunkPos, ChunkPosHash> dirtyChunks;
    for (auto it = chunkRenderMeshes.begin(); it != chunkRenderMeshes.end();)
    {
        if (!world.getChunk(it->first))
        {
            const ChunkPos removed = it->first;
            dirtyChunks.insert({removed.x - 1, removed.z});
            dirtyChunks.insert({removed.x + 1, removed.z});
            dirtyChunks.insert({removed.x, removed.z - 1});
            dirtyChunks.insert({removed.x, removed.z + 1});
            glDeleteVertexArrays(1, &it->second.vao);
            glDeleteBuffers(1, &it->second.vbo);
            it = chunkRenderMeshes.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& entry : world.getChunks())
    {
        const ChunkPos pos = entry.first;
        if (chunkRenderMeshes.find(pos) == chunkRenderMeshes.end())
        {
            dirtyChunks.insert(pos);
            dirtyChunks.insert({pos.x - 1, pos.z});
            dirtyChunks.insert({pos.x + 1, pos.z});
            dirtyChunks.insert({pos.x, pos.z - 1});
            dirtyChunks.insert({pos.x, pos.z + 1});
        }
    }

    for (const ChunkPos& pos : dirtyChunks)
    {
        if (world.getChunk(pos))
            updateChunkMesh(pos);
    }
}

void clearChunkMeshes()
{
    for (auto& entry : chunkRenderMeshes)
    {
        glDeleteVertexArrays(1, &entry.second.vao);
        glDeleteBuffers(1, &entry.second.vbo);
    }
    chunkRenderMeshes.clear();
}

void updateBlockMeshes(const BlockPos& pos)
{
    const ChunkAddress address = WorldCoordinates::toChunkAddress(pos);
    std::unordered_set<ChunkPos, ChunkPosHash> dirtyChunks;
    dirtyChunks.insert(address.chunk);
    if (address.local.x == 0)
        dirtyChunks.insert({address.chunk.x - 1, address.chunk.z});
    if (address.local.x == WorldConfig::CHUNK_SIZE - 1)
        dirtyChunks.insert({address.chunk.x + 1, address.chunk.z});
    if (address.local.z == 0)
        dirtyChunks.insert({address.chunk.x, address.chunk.z - 1});
    if (address.local.z == WorldConfig::CHUNK_SIZE - 1)
        dirtyChunks.insert({address.chunk.x, address.chunk.z + 1});

    for (const ChunkPos& chunkPos : dirtyChunks)
    {
        if (world.getChunk(chunkPos))
            updateChunkMesh(chunkPos);
    }
}

int surfaceHeight(int x, int z)
{
    for (int y = WorldConfig::WORLD_HEIGHT - 1; y >= 0; --y)
    {
        if (world.getBlock({x, y, z}) != BlockId::Air)
            return y + 1;
    }
    return 1;
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
        vertices.push_back({p[edge[0]], color, glm::vec2(0.0f)});
        vertices.push_back({p[edge[1]], color, glm::vec2(0.0f)});
    }
}

int main(int argc, char* argv[])
{
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex)
    {
        const std::string argument = argv[argumentIndex];
        if (argument == "--version")
        {
            std::cout << MINECRAFTCOPY_VERSION << '\n';
            return 0;
        }
        if (argument == "--world=infinite")
            infiniteWorld = true;
        else if (argument == "--world=finite")
            infiniteWorld = false;
    }

    world = World(infiniteWorld ? WorldType::Infinite : WorldType::Limited);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(
        WIDTH, HEIGHT,
        infiniteWorld ? "MinecraftCopy - Infinite World" : "MinecraftCopy - Finite World",
        nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwSetWindowFocusCallback(window, focus_callback);
    glfwFocusWindow(window);
    windowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    setCursorLock(window, true);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

    // Initialize GLAD and report the actual context version.
    int glMajor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    int glMinor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);

    std::cerr << "Requested OpenGL context: "
              << glMajor << "." << glMinor << '\n';

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0)
    {
        std::cerr << "GLAD failed to load OpenGL functions.\n";

        const char *description = nullptr;
        int errorCode = glfwGetError(&description);
        std::cerr << "GLFW error: " << errorCode
                  << " " << (description ? description : "none") << '\n';

        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "GLAD initialized successfully.\n";

    glEnable(GL_DEPTH_TEST);

    // Load the texture atlas.
    unsigned int atlasTexture = 0;
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    int imageWidth = 0;
    int imageHeight = 0;
    int channels = 0;

    std::filesystem::path assetPath = "assets/textureatlas.png";
    if (argc > 0)
    {
        const std::filesystem::path executableAssetPath =
            std::filesystem::path(argv[0]).parent_path() / "assets" / "textureatlas.png";
        if (std::filesystem::exists(executableAssetPath))
            assetPath = executableAssetPath;
    }

    const std::string assetPathString = assetPath.string();
    unsigned char *image = stbi_load(
        assetPathString.c_str(),
        &imageWidth,
        &imageHeight,
        &channels,
        4);

    if (!image)
    {
        std::cerr << "Failed to load " << assetPathString << ": "
                  << stbi_failure_reason() << '\n';
        glDeleteTextures(1, &atlasTexture);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        imageWidth,
        imageHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image);

    stbi_image_free(image);

    glEnable(GL_DEPTH_TEST);

    const ChunkPos initialCenter{12, 12};
    const int spawnX = infiniteWorld
        ? 0
        : initialCenter.x * WorldConfig::CHUNK_SIZE + 8;
    const int spawnZ = infiniteWorld
        ? 0
        : initialCenter.z * WorldConfig::CHUNK_SIZE + 8;
    if (infiniteWorld)
    {
        ChunkStreamer::update(world, terrainGenerator, spawnX, spawnZ, 4);
    }
    else
    {
        for (int x = 0; x < WorldConfig::LIMITED_CHUNKS_X; ++x)
            for (int z = 0; z < WorldConfig::LIMITED_CHUNKS_Z; ++z)
                terrainGenerator.generateChunk(world, {x, z});
    }
    player.setPosition(glm::vec3(
        spawnX + 0.5f,
        static_cast<float>(surfaceHeight(spawnX, spawnZ)) + 1.8f,
        spawnZ + 0.5f));

    unsigned int shaderProgram = makeShaderProgram();
    if (!shaderProgram)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    syncChunkMeshes();

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

    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void *>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    bool leftMouseWasDown = false;
    bool rightMouseWasDown = false;
    bool modeKeyWasDown = false;
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = std::min(currentFrame - lastFrame, 0.05f);
        lastFrame = currentFrame;

        glfwPollEvents();

        windowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
        const bool gameplayActive = cursorLockRequested && windowFocused &&
                                    !suppressGameplayForResumeFrame;
        const bool modeKeyDown =
            glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
        BlockTarget target;

        if (gameplayActive)
        {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            if (modeKeyDown && !modeKeyWasDown)
            {
                infiniteWorld = !infiniteWorld;
                glfwSetWindowTitle(window, infiniteWorld
                    ? "MinecraftCopy - Infinite World"
                    : "MinecraftCopy - Finite World");
                clearChunkMeshes();
                world.clear();
                world = World(infiniteWorld ? WorldType::Infinite : WorldType::Limited);
                if (infiniteWorld)
                {
                    ChunkStreamer::update(
                        world, terrainGenerator,
                        static_cast<int>(std::floor(player.getPosition().x)),
                        static_cast<int>(std::floor(player.getPosition().z)), 4);
                }
                else
                {
                    for (int x = 0; x < WorldConfig::LIMITED_CHUNKS_X; ++x)
                        for (int z = 0; z < WorldConfig::LIMITED_CHUNKS_Z; ++z)
                            terrainGenerator.generateChunk(world, {x, z});
                }
                const int px = infiniteWorld
                    ? static_cast<int>(std::floor(player.getPosition().x))
                    : initialCenter.x * WorldConfig::CHUNK_SIZE + 8;
                const int pz = infiniteWorld
                    ? static_cast<int>(std::floor(player.getPosition().z))
                    : initialCenter.z * WorldConfig::CHUNK_SIZE + 8;
                player.setPosition(glm::vec3(
                    px + 0.5f, surfaceHeight(px, pz) + 1.8f, pz + 0.5f));
                syncChunkMeshes();
            }

            if (infiniteWorld && ChunkStreamer::update(
                    world, terrainGenerator,
                    static_cast<int>(std::floor(player.getPosition().x)),
                    static_cast<int>(std::floor(player.getPosition().z)), 4))
            {
                syncChunkMeshes();
            }

            player.processMovement(window, deltaTime, world);
            target = raycastBlocks(
                world, player.getPosition(), player.getFront());

            const bool leftMouseDown = glfwGetMouseButton(
                window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (leftMouseDown && !leftMouseWasDown && target.hit)
            {
                if (world.setBlock(
                        {target.x, target.y, target.z}, BlockId::Air))
                {
                    updateBlockMeshes({target.x, target.y, target.z});
                }
            }
            leftMouseWasDown = leftMouseDown;

            const bool rightMouseDown = glfwGetMouseButton(
                window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (rightMouseDown && !rightMouseWasDown && target.hit)
            {
                const glm::ivec3 place = target.previousBlock;
                if (place.y >= 0 && place.y < WorldConfig::WORLD_HEIGHT &&
                    world.isInsideWorld(place.x, place.z) &&
                    world.getBlock({place.x, place.y, place.z}) == BlockId::Air)
                {
                    const glm::vec3 p = player.getPosition();
                    const bool overlaps =
                        p.x + 0.3f > place.x && p.x - 0.3f < place.x + 1 &&
                        p.y > place.y && p.y - 1.8f < place.y + 1 &&
                        p.z + 0.3f > place.z && p.z - 0.3f < place.z + 1;

                    if (!overlaps)
                    {
                        world.setBlock(
                            {place.x, place.y, place.z}, BlockId::Grass);
                        updateBlockMeshes({place.x, place.y, place.z});
                    }
                }
            }
            rightMouseWasDown = rightMouseDown;
        }
        else
        {
            // Consume polled input while paused so held keys and clicks do not
            // trigger immediately after the cursor is locked again.
            modeKeyWasDown = modeKeyDown;
            leftMouseWasDown = glfwGetMouseButton(
                window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            rightMouseWasDown = glfwGetMouseButton(
                window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            suppressGameplayForResumeFrame = false;
        }

        modeKeyWasDown = modeKeyDown;

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

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlasTexture);

        glUniform1i(
            glGetUniformLocation(shaderProgram, "textureAtlas"), 0);

        glUniform1i(
            glGetUniformLocation(shaderProgram, "useTexture"), GL_TRUE);

        GLint mvpLocation =
            glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(
            mvpLocation, 1, GL_FALSE, &MVP[0][0]);

        for (const auto& entry : chunkRenderMeshes)
        {
            if (entry.second.vertexCount == 0)
                continue;
            glBindVertexArray(entry.second.vao);
            glDrawArrays(GL_TRIANGLES, 0, entry.second.vertexCount);
        }

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

            glUniform1i(
                glGetUniformLocation(shaderProgram, "useTexture"), GL_FALSE);

            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0,
                         static_cast<GLsizei>(outlineVertices.size()));

            glUniform1i(
                glGetUniformLocation(shaderProgram, "useTexture"), GL_TRUE);

            glBindVertexArray(0);
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

    clearChunkMeshes();
    glDeleteProgram(shaderProgram);
    glDeleteTextures(1, &atlasTexture);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
