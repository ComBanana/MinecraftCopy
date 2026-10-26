#include "UpdateService.h"

#include "GameLauncher.h"
#include "ReleaseManifest.h"
#include "Versioning.h"

#include <Windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace
{
using nlohmann::json;
constexpr char kReleaseApi[] = "https://api.github.com/repos/ComBanana/MinecraftCopy/releases?per_page=100";
constexpr char kManifestName[] = "game-manifest-windows-x64.json";
constexpr char kExecutableName[] = "MinecraftCopy.exe";
constexpr char kTextureName[] = "textureatlas.png";
constexpr std::size_t kMaxMetadataSize = 2 * 1024 * 1024;

struct HttpHandle
{
    HINTERNET value = nullptr;
    ~HttpHandle()
    {
        if (value)
            WinHttpCloseHandle(value);
    }
    HttpHandle(const HttpHandle &) = delete;
    HttpHandle &operator=(const HttpHandle &) = delete;
    HttpHandle() = default;
    explicit HttpHandle(HINTERNET handle) : value(handle) {}
};

struct ParsedUrl
{
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

std::wstring widenAscii(const std::string &value)
{
    return std::wstring(value.begin(), value.end());
}

std::string narrowAscii(const std::wstring &value)
{
    std::string output;
    output.reserve(value.size());
    for (wchar_t character : value)
        output.push_back(static_cast<char>(character));
    return output;
}

bool parseHttpsUrl(const std::string &url, ParsedUrl &parsed)
{
    const std::wstring wideUrl = widenAscii(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components) ||
        components.nScheme != INTERNET_SCHEME_HTTPS)
        return false;

    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0)
        parsed.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    parsed.port = components.nPort;
    return !parsed.host.empty() && !parsed.path.empty();
}

bool request(const std::string &url, bool apiRequest, std::size_t maxBytes,
             const std::filesystem::path *destination, std::uint64_t expectedSize,
             std::string &body, std::wstring &error)
{
    ParsedUrl parsed;
    if (!parseHttpsUrl(url, parsed))
    {
        error = L"GitHub returned an invalid HTTPS download address.";
        return false;
    }

    HttpHandle session{WinHttpOpen(L"MinecraftCopyLauncher",
                                   WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS,
                                   0)};
    if (!session.value)
    {
        error = L"Windows could not start the secure download service.";
        return false;
    }
    WinHttpSetTimeouts(session.value, 15000, 15000, 30000, 120000);

    HttpHandle connection{WinHttpConnect(session.value, parsed.host.c_str(), parsed.port, 0)};
    if (!connection.value)
    {
        error = L"Could not connect to GitHub. Check your internet connection.";
        return false;
    }
    HttpHandle httpRequest{WinHttpOpenRequest(connection.value,
                                              L"GET",
                                              parsed.path.c_str(),
                                              nullptr,
                                              WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              WINHTTP_FLAG_SECURE)};
    if (!httpRequest.value)
    {
        error = L"Could not open a secure GitHub request.";
        return false;
    }
    if (apiRequest)
    {
        WinHttpAddRequestHeaders(httpRequest.value,
            L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n",
            static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(httpRequest.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(httpRequest.value, nullptr))
    {
        error = L"GitHub could not be reached. The repository must be public and have a published release.";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(httpRequest.value,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &statusCode,
                             &statusSize,
                             WINHTTP_NO_HEADER_INDEX) || statusCode != 200)
    {
        if (statusCode == 404)
            error = L"GitHub releases are not available. Make the repository public and publish a release first.";
        else
            error = L"GitHub returned an unsuccessful response.";
        return false;
    }

    std::ofstream file;
    if (destination)
    {
        file.open(*destination, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            error = L"Could not create a temporary game download file.";
            return false;
        }
    }

    std::uint64_t received = 0;
    std::array<char, 64 * 1024> buffer{};
    while (true)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(httpRequest.value, &available))
        {
            error = L"The GitHub download was interrupted.";
            return false;
        }
        if (available == 0)
            break;
        const DWORD toRead = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
        DWORD bytesRead = 0;
        if (!WinHttpReadData(httpRequest.value, buffer.data(), toRead, &bytesRead) || bytesRead == 0)
        {
            error = L"The GitHub download was interrupted.";
            return false;
        }
        received += bytesRead;
        if ((destination && received > expectedSize) || (!destination && received > maxBytes))
        {
            error = L"The GitHub release file is larger than its published size.";
            return false;
        }
        if (destination)
        {
            file.write(buffer.data(), bytesRead);
            if (!file)
            {
                error = L"Could not write the downloaded game file.";
                return false;
            }
        }
        else
        {
            body.append(buffer.data(), bytesRead);
        }
    }
    if (destination)
    {
        file.close();
        if (received != expectedSize)
        {
            error = L"The downloaded game file did not match its published size.";
            return false;
        }
    }
    return true;
}

std::string sha256File(const std::filesystem::path &path, std::wstring &error)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    {
        error = L"Windows could not verify the downloaded game.";
        return {};
    }
    DWORD objectLength = 0;
    DWORD resultLength = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &resultLength, 0) < 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        error = L"Windows could not verify the downloaded game.";
        return {};
    }
    std::vector<UCHAR> object(objectLength);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) < 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        error = L"Windows could not verify the downloaded game.";
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    std::array<char, 64 * 1024> buffer{};
    while (file)
    {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0) < 0)
        {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            error = L"Windows could not verify the downloaded game.";
            return {};
        }
    }
    if (!file.eof())
    {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        error = L"Could not read the downloaded game for verification.";
        return {};
    }

    std::array<UCHAR, 32> digest{};
    if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
    {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        error = L"Windows could not verify the downloaded game.";
        return {};
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    constexpr char hex[] = "0123456789abcdef";
    std::string output;
    output.reserve(digest.size() * 2);
    for (UCHAR byte : digest)
    {
        output.push_back(hex[byte >> 4]);
        output.push_back(hex[byte & 0x0f]);
    }
    return output;
}

std::string removeTagPrefix(std::string tag)
{
    if (!tag.empty() && tag.front() == 'v')
        tag.erase(tag.begin());
    return tag;
}

bool findLatestPackageImpl(ReleasePackage &package, std::wstring &error)
{
    std::string response;
    if (!request(kReleaseApi, true, kMaxMetadataSize, nullptr, 0, response, error))
        return false;
    json releases;
    try
    {
        releases = json::parse(response);
    }
    catch (const json::exception &)
    {
        error = L"GitHub returned invalid release information.";
        return false;
    }
    if (!releases.is_array())
    {
        error = L"GitHub returned invalid release information.";
        return false;
    }

    bool found = false;
    std::string latestVersion;
    json latestRelease;
    for (const json &release : releases)
    {
        if (!release.is_object() || release.value("draft", true))
            continue;
        const std::string version = removeTagPrefix(release.value("tag_name", std::string{}));
        if (!isValidVersion(version))
            continue;
        const json assets = release.value("assets", json::array());
        if (!assets.is_array())
            continue;
        bool hasManifest = false;
        for (const json &asset : assets)
        {
            if (asset.is_object() && asset.value("name", std::string{}) == kManifestName)
                hasManifest = true;
        }
        if (!hasManifest)
            continue;
        if (!found || compareVersions(version, latestVersion) > 0)
        {
            found = true;
            latestVersion = version;
            latestRelease = release;
        }
    }
    if (!found)
    {
        error = L"No compatible Windows game release has been published yet.";
        return false;
    }

    std::map<std::string, std::string> assetUrls;
    const std::string latestTag = latestRelease.value("tag_name", std::string{});
    for (const json &asset : latestRelease["assets"])
    {
        if (!asset.is_object())
            continue;
        const std::string name = asset.value("name", std::string{});
        const std::string url = asset.value("browser_download_url", std::string{});
        if (name == kManifestName || name == kExecutableName || name == kTextureName)
        {
            const std::string expectedUrl =
                "https://github.com/ComBanana/MinecraftCopy/releases/download/" + latestTag + "/" + name;
            if (url != expectedUrl)
            {
                error = L"The GitHub release contains an invalid download address.";
                return false;
            }
            assetUrls[name] = url;
        }
    }
    if (!assetUrls.count(kManifestName) || !assetUrls.count(kExecutableName) || !assetUrls.count(kTextureName))
    {
        error = L"The latest Windows release is missing required game files.";
        return false;
    }

    std::string manifestText;
    if (!request(assetUrls[kManifestName], false, kMaxMetadataSize, nullptr, 0, manifestText, error))
        return false;
    return parseReleaseManifest(manifestText, latestVersion,
                                assetUrls.at(kExecutableName), assetUrls.at(kTextureName),
                                package, error);
}

bool findLatestPackage(ReleasePackage &package, std::wstring &error)
{
    try
    {
        return findLatestPackageImpl(package, error);
    }
    catch (const json::exception &)
    {
        error = L"GitHub returned release information in an unexpected format.";
        return false;
    }
}

std::filesystem::path appDataRoot()
{
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (length == 0)
        return {};
    std::vector<wchar_t> buffer(length);
    length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
        return {};
    return std::filesystem::path(std::wstring(buffer.data(), length)) / L"MinecraftCopy";
}

bool verifyFile(const std::filesystem::path &path, const ReleasePackageFile &file, std::wstring &error)
{
    std::error_code ec;
    const std::uint64_t actualSize = std::filesystem::file_size(path, ec);
    if (ec || actualSize != file.size)
    {
        error = L"A downloaded game file has the wrong size.";
        return false;
    }
    const std::string actualHash = sha256File(path, error);
    if (actualHash.empty())
        return false;
    if (actualHash != file.sha256)
    {
        error = L"A downloaded game file failed its SHA-256 check.";
        return false;
    }
    return true;
}

bool writeCurrentInstall(const std::filesystem::path &root,
                         const std::string &version,
                         const std::string &folder,
                         std::wstring &error)
{
    const json current{{"schema", 1}, {"version", version}, {"folder", folder}};
    const std::filesystem::path currentPath = root / L"current.json";
    const std::filesystem::path temporaryPath = root / (L"current-" + std::to_wstring(GetCurrentProcessId()) + L".tmp");
    {
        std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
        const std::string text = current.dump(2);
        if (!file || !(file.write(text.data(), static_cast<std::streamsize>(text.size()))))
        {
            error = L"Could not save the selected game version.";
            return false;
        }
    }
    if (!MoveFileExW(temporaryPath.c_str(), currentPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        error = L"Could not activate the newly installed game.";
        return false;
    }
    return true;
}

bool resolveCurrentInstall(const std::filesystem::path &root,
                           std::filesystem::path &gamePath,
                           std::string &version)
{
    try
    {
        std::ifstream file(root / L"current.json", std::ios::binary);
        if (!file)
            return false;
        const json current = json::parse(file);
        if (!current.is_object() || current.value("schema", 0) != 1)
            return false;
        version = current.value("version", std::string{});
        const std::string folder = current.value("folder", std::string{});
        if (!isValidVersion(version) || folder.rfind(version + "-", 0) != 0)
            return false;
        const std::string suffix = folder.substr(version.size() + 1);
        // The directory suffix contains two short SHA-256 prefixes, never a user supplied path.
        if (suffix.size() != 25 || suffix[12] != '-')
            return false;
        for (std::size_t i = 0; i < suffix.size(); ++i)
        {
            if (i == 12)
                continue;
            const char value = suffix[i];
            if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f')))
                return false;
        }
        const std::filesystem::path installDirectory = root / L"versions" / widenAscii(folder);
        gamePath = installDirectory / kExecutableName;
        const std::filesystem::path texturePath = installDirectory / L"assets" / L"textureatlas.png";
        if (!std::filesystem::is_regular_file(texturePath))
            return false;
        const VersionResult actual = readGameVersion(gamePath);
        return actual.success && actual.version == widenAscii(version);
    }
    catch (const std::exception &)
    {
        return false;
    }
}
}

UpdateService::UpdateService(std::filesystem::path launcherDirectory)
    : m_launcherDirectory(std::move(launcherDirectory))
{
}

std::filesystem::path UpdateService::localInstallRoot() const
{
    return appDataRoot();
}

std::filesystem::path UpdateService::installedGameExecutable() const
{
    std::filesystem::path gamePath;
    std::string version;
    if (resolveCurrentInstall(localInstallRoot(), gamePath, version))
        return gamePath;
    return {};
}

std::filesystem::path UpdateService::gameExecutable() const
{
    const std::filesystem::path installedGame = installedGameExecutable();
#if defined(MINECRAFTCOPY_DEVELOPMENT_BUILD)
    const std::filesystem::path localGame = m_launcherDirectory / L"MinecraftCopy.exe";
    if (std::filesystem::is_regular_file(localGame))
    {
        if (installedGame.empty())
            return localGame;

        const VersionResult localVersion = readGameVersion(localGame);
        const VersionResult installedVersion = readGameVersion(installedGame);
        if (!installedVersion.success)
            return localGame;
        if (!localVersion.success || compareVersions(narrowAscii(installedVersion.version), narrowAscii(localVersion.version)) > 0)
            return installedGame;
        return localGame;
    }
#endif
    return installedGame;
}

GameUpdateState UpdateService::checkForUpdates() const
{
    GameUpdateState state;
    const std::filesystem::path localGame = gameExecutable();
    if (!localGame.empty())
    {
        const VersionResult installed = readGameVersion(localGame);
        if (installed.success)
        {
            state.gameInstalled = true;
            state.installedVersion = installed.version;
        }
    }

    ReleasePackage package;
    std::wstring error;
    if (!findLatestPackage(package, error))
    {
        state.message = error;
        return state;
    }
    state.updateCheckSucceeded = true;
    state.latestVersion = widenAscii(package.version);
    if (!state.gameInstalled)
    {
        state.updateAvailable = true;
        state.message = L"MinecraftCopy is ready to install.";
    }
    else
    {
        const std::string installed = narrowAscii(state.installedVersion);
        const int comparison = compareVersions(package.version, installed);
        state.updateAvailable = comparison > 0;
        if (comparison > 0)
            state.message = L"A newer game version is available.";
        else if (comparison == 0)
            state.message = L"The game is up to date.";
        else
            state.message = L"The installed game is newer than the published release.";
    }
    return state;
}

GameUpdateResult UpdateService::installLatest()
{
    GameUpdateResult result;
    const std::filesystem::path installedPath = gameExecutable();
    if (!installedPath.empty())
    {
        const VersionResult installedVersion = readGameVersion(installedPath);
        if (installedVersion.success)
        {
            result.state.gameInstalled = true;
            result.state.installedVersion = installedVersion.version;
        }
    }
    ReleasePackage package;
    if (!findLatestPackage(package, result.state.message))
        return result;

    const std::filesystem::path root = localInstallRoot();
    if (root.empty())
    {
        result.state.message = L"Windows did not provide a local application data folder.";
        return result;
    }
    const std::filesystem::path versionRoot = root / L"versions";
    std::error_code ec;
    std::filesystem::create_directories(versionRoot, ec);
    if (ec)
    {
        result.state.message = L"Could not create the game installation folder.";
        return result;
    }

    const std::wstring stagingName = L"stage-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const std::filesystem::path staging = root / stagingName;
    if (!std::filesystem::create_directory(staging, ec) || ec)
    {
        result.state.message = L"Could not prepare a temporary game installation folder.";
        return result;
    }
    const auto cleanStaging = [&]()
    {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
    };

    const std::filesystem::path executablePath = staging / kExecutableName;
    const std::filesystem::path textureDirectory = staging / L"assets";
    std::filesystem::create_directory(textureDirectory, ec);
    if (ec)
    {
        cleanStaging();
        result.state.message = L"Could not prepare the game assets folder.";
        return result;
    }
    const std::filesystem::path texturePath = textureDirectory / kTextureName;

    std::string unusedBody;
    if (!request(package.executable.url, false, 0, &executablePath, package.executable.size, unusedBody, result.state.message) ||
        !request(package.texture.url, false, 0, &texturePath, package.texture.size, unusedBody, result.state.message))
    {
        cleanStaging();
        return result;
    }
    if (!verifyFile(executablePath, package.executable, result.state.message) ||
        !verifyFile(texturePath, package.texture, result.state.message))
    {
        cleanStaging();
        return result;
    }
    const VersionResult installed = readGameVersion(executablePath);
    if (!installed.success || installed.version != widenAscii(package.version))
    {
        cleanStaging();
        result.state.message = L"The downloaded executable does not report the release version.";
        return result;
    }

    const std::string folder = package.version + "-" + package.executable.sha256.substr(0, 12) + "-" + package.texture.sha256.substr(0, 12);
    const std::filesystem::path destination = versionRoot / widenAscii(folder);
    if (std::filesystem::exists(destination))
    {
        std::wstring verifyError;
        const VersionResult existingVersion = readGameVersion(destination / kExecutableName);
        if (!verifyFile(destination / kExecutableName, package.executable, verifyError) ||
            !verifyFile(destination / L"assets" / L"textureatlas.png", package.texture, verifyError) ||
            !existingVersion.success || existingVersion.version != widenAscii(package.version))
        {
            cleanStaging();
            result.state.message = L"An existing copy of this release failed verification.";
            return result;
        }
        cleanStaging();
    }
    else if (!MoveFileExW(staging.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
    {
        cleanStaging();
        result.state.message = L"Could not place the verified game files.";
        return result;
    }

    if (!writeCurrentInstall(root, package.version, folder, result.state.message))
        return result;

    result.success = true;
    result.state.gameInstalled = true;
    result.state.updateCheckSucceeded = true;
    result.state.updateAvailable = false;
    result.state.installedVersion = widenAscii(package.version);
    result.state.latestVersion = widenAscii(package.version);
    result.state.message = L"MinecraftCopy " + result.state.installedVersion + L" is installed.";
    return result;
}
