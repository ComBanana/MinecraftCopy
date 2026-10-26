
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

enum class WorldType
{
    Limited,
    Infinite
};

enum class BlockId : std::uint16_t
{
    Air = 0,
    Grass,
    Dirt,
    Stone
};

namespace WorldConfig
{
    constexpr int CHUNK_SIZE = 16;
    constexpr int WORLD_HEIGHT = 256;

    // A 25 x 25 chunk limited world: 400 x 400 blocks.
    constexpr int LIMITED_CHUNKS_X = 25;
    constexpr int LIMITED_CHUNKS_Z = 25;

    constexpr int LIMITED_BLOCKS_X =
        LIMITED_CHUNKS_X * CHUNK_SIZE;

    constexpr int LIMITED_BLOCKS_Z =
        LIMITED_CHUNKS_Z * CHUNK_SIZE;
}

struct ChunkPos
{
    int x = 0;
    int z = 0;

    bool operator==(const ChunkPos& other) const noexcept
    {
        return x == other.x && z == other.z;
    }
};

struct ChunkPosHash
{
    std::size_t operator()(const ChunkPos& pos) const noexcept
    {
        const std::size_t hx = std::hash<int>{}(pos.x);
        const std::size_t hz = std::hash<int>{}(pos.z);

        return hx ^ (hz + 0x9e3779b9u + (hx << 6) + (hx >> 2));
    }
};

struct BlockPos
{
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const BlockPos& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct BlockPosHash
{
    std::size_t operator()(const BlockPos& pos) const noexcept
    {
        const std::size_t hx = std::hash<int>{}(pos.x);
        const std::size_t hy = std::hash<int>{}(pos.y);
        const std::size_t hz = std::hash<int>{}(pos.z);
        return hx ^ (hy + 0x9e3779b9u + (hx << 6) + (hx >> 2)) ^
               (hz + 0x9e3779b9u + (hy << 6) + (hy >> 2));
    }
};

struct LocalBlockPos
{
    int x = 0;
    int y = 0;
    int z = 0;
};

struct ChunkAddress
{
    ChunkPos chunk;
    LocalBlockPos local;
};

namespace WorldCoordinates
{
    // Correctly handles negative world coordinates.
    inline int floorDiv(int value, int divisor)
    {
        int quotient = value / divisor;
        const int remainder = value % divisor;

        if (remainder < 0)
            --quotient;

        return quotient;
    }

    inline int positiveMod(int value, int divisor)
    {
        const int remainder = value % divisor;
        return remainder < 0 ? remainder + divisor : remainder;
    }

    inline ChunkAddress toChunkAddress(const BlockPos& pos)
    {
        return {
            {
                floorDiv(pos.x, WorldConfig::CHUNK_SIZE),
                floorDiv(pos.z, WorldConfig::CHUNK_SIZE)
            },
            {
                positiveMod(pos.x, WorldConfig::CHUNK_SIZE),
                pos.y,
                positiveMod(pos.z, WorldConfig::CHUNK_SIZE)
            }
        };
    }
}

class Chunk
{
public:
    static constexpr std::size_t BLOCK_COUNT =
        static_cast<std::size_t>(WorldConfig::CHUNK_SIZE) *
        WorldConfig::WORLD_HEIGHT *
        WorldConfig::CHUNK_SIZE;

    Chunk()
    {
        blocks.fill(BlockId::Air);
    }

    BlockId getBlock(int x, int y, int z) const
    {
        if (!isValidPosition(x, y, z))
            return BlockId::Air;

        return blocks[indexOf(x, y, z)];
    }

    bool setBlock(int x, int y, int z, BlockId block)
    {
        if (!isValidPosition(x, y, z))
            return false;

        blocks[indexOf(x, y, z)] = block;
        return true;
    }

private:
    std::array<BlockId, BLOCK_COUNT> blocks{};

    static bool isValidPosition(int x, int y, int z)
    {
        return x >= 0 &&
               x < WorldConfig::CHUNK_SIZE &&
               y >= 0 &&
               y < WorldConfig::WORLD_HEIGHT &&
               z >= 0 &&
               z < WorldConfig::CHUNK_SIZE;
    }

    static constexpr std::size_t indexOf(int x, int y, int z)
    {
        return
            (static_cast<std::size_t>(y) *
                 WorldConfig::CHUNK_SIZE +
             static_cast<std::size_t>(z)) *
                WorldConfig::CHUNK_SIZE +
            static_cast<std::size_t>(x);
    }
};

class World
{
public:
    explicit World(WorldType type = WorldType::Limited)
        : worldType(type)
    {
    }

    WorldType getType() const noexcept
    {
        return worldType;
    }

    bool isInsideWorld(int x, int z) const noexcept
    {
        if (worldType == WorldType::Infinite)
            return true;

        // Limited worlds use coordinates from 0 to 399.
        return x >= 0 &&
               x < WorldConfig::LIMITED_BLOCKS_X &&
               z >= 0 &&
               z < WorldConfig::LIMITED_BLOCKS_Z;
    }

    Chunk* getChunk(const ChunkPos& pos)
    {
        auto it = chunks.find(pos);
        return it == chunks.end() ? nullptr : &it->second;
    }

    const Chunk* getChunk(const ChunkPos& pos) const
    {
        auto it = chunks.find(pos);
        return it == chunks.end() ? nullptr : &it->second;
    }

    Chunk& ensureChunk(const ChunkPos& pos)
    {
        if (worldType == WorldType::Limited)
        {
            const bool valid =
                pos.x >= 0 &&
                pos.x < WorldConfig::LIMITED_CHUNKS_X &&
                pos.z >= 0 &&
                pos.z < WorldConfig::LIMITED_CHUNKS_Z;

            if (!valid)
                throw std::out_of_range(
                    "Chunk is outside the limited world.");
        }

        return chunks.try_emplace(pos).first->second;
    }

    BlockId getBlock(const BlockPos& pos) const
    {
        if (pos.y < 0 || pos.y >= WorldConfig::WORLD_HEIGHT)
            return BlockId::Air;

        if (!isInsideWorld(pos.x, pos.z))
            return BlockId::Air;

        const ChunkAddress address =
            WorldCoordinates::toChunkAddress(pos);

        const Chunk* chunk = getChunk(address.chunk);

        // Missing chunks are treated as empty until generated.
        if (!chunk)
            return BlockId::Air;

        return chunk->getBlock(
            address.local.x,
            address.local.y,
            address.local.z);
    }

    bool setBlock(const BlockPos& pos, BlockId block)
    {
        if (!setBlockValue(pos, block))
            return false;

        blockOverrides[pos] = block;
        return true;
    }

    bool setGeneratedBlock(const BlockPos& pos, BlockId block)
    {
        return setBlockValue(pos, block);
    }

    void applyBlockOverrides(const ChunkPos& chunkPos)
    {
        Chunk* chunk = getChunk(chunkPos);
        if (!chunk)
            return;

        for (const auto& [pos, block] : blockOverrides)
        {
            const ChunkAddress address = WorldCoordinates::toChunkAddress(pos);
            if (address.chunk == chunkPos)
            {
                chunk->setBlock(address.local.x, address.local.y,
                                address.local.z, block);
            }
        }
    }

private:
    bool setBlockValue(const BlockPos& pos, BlockId block)
    {
        if (pos.y < 0 || pos.y >= WorldConfig::WORLD_HEIGHT)
            return false;

        if (!isInsideWorld(pos.x, pos.z))
            return false;

        const ChunkAddress address =
            WorldCoordinates::toChunkAddress(pos);

        Chunk& chunk = ensureChunk(address.chunk);

        return chunk.setBlock(
            address.local.x,
            address.local.y,
            address.local.z,
            block);
    }

public:

    std::size_t getLoadedChunkCount() const noexcept
    {
        return chunks.size();
    }

    const std::unordered_map<ChunkPos, Chunk, ChunkPosHash>& getChunks() const noexcept
    {
        return chunks;
    }

    bool removeChunk(const ChunkPos& pos)
    {
        return chunks.erase(pos) != 0;
    }

    void clear() noexcept
    {
        chunks.clear();
        blockOverrides.clear();
    }

private:
    WorldType worldType;

    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> chunks;
    std::unordered_map<BlockPos, BlockId, BlockPosHash> blockOverrides;
};
