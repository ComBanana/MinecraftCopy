#pragma once

#include <filesystem>
#include <string>

enum class WorldMode
{
    Finite,
    Infinite
};

struct LaunchResult
{
    bool success = false;
    std::wstring message;
};

struct VersionResult
{
    bool success = false;
    std::wstring version;
    std::wstring error;
};

VersionResult readGameVersion(const std::filesystem::path &gamePath);
LaunchResult launchGame(const std::filesystem::path &gamePath, WorldMode mode);
