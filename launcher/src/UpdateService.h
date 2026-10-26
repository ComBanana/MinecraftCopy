#pragma once

#include <filesystem>
#include <string>

struct GameUpdateState
{
    bool gameInstalled = false;
    bool updateCheckSucceeded = false;
    bool updateAvailable = false;
    std::wstring installedVersion;
    std::wstring latestVersion;
    std::wstring message;
};

struct GameUpdateResult
{
    bool success = false;
    GameUpdateState state;
};

class UpdateService
{
public:
    explicit UpdateService(std::filesystem::path launcherDirectory);

    std::filesystem::path gameExecutable() const;
    GameUpdateState checkForUpdates() const;
    GameUpdateResult installLatest();

private:
    std::filesystem::path installedGameExecutable() const;
    std::filesystem::path localInstallRoot() const;

    std::filesystem::path m_launcherDirectory;
};
