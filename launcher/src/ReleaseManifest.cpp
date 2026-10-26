#include "ReleaseManifest.h"

#include "Versioning.h"

#include <json.hpp>

#include <algorithm>
#include <string>
#include <utility>

namespace
{
using nlohmann::json;
constexpr char kAssetUrlPrefix[] = "https://github.com/ComBanana/MinecraftCopy/releases/download/v";
constexpr char kExecutableName[] = "MinecraftCopy.exe";
constexpr char kTextureName[] = "textureatlas.png";
constexpr std::uint64_t kMaxReleaseAssetSize = 512ull * 1024ull * 1024ull;

bool validHash(const std::string &hash)
{
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char value)
    {
        return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
    });
}

std::uint64_t boundedSize(const json &value)
{
    if (value.is_number_unsigned())
    {
        const std::uint64_t size = value.get<std::uint64_t>();
        return size > 0 && size <= kMaxReleaseAssetSize ? size : 0;
    }
    if (value.is_number_integer())
    {
        const std::int64_t size = value.get<std::int64_t>();
        return size > 0 && static_cast<std::uint64_t>(size) <= kMaxReleaseAssetSize
            ? static_cast<std::uint64_t>(size)
            : 0;
    }
    return 0;
}
}

bool parseReleaseManifest(std::string_view manifestText,
                          std::string_view expectedVersion,
                          std::string_view executableUrl,
                          std::string_view textureUrl,
                          ReleasePackage &package,
                          std::wstring &error)
{
    if (!isValidVersion(expectedVersion))
    {
        error = L"The GitHub release has an invalid version.";
        return false;
    }

    const std::string versionPath = std::string(kAssetUrlPrefix) + std::string(expectedVersion) + "/";
    if (executableUrl != versionPath + kExecutableName || textureUrl != versionPath + kTextureName)
    {
        error = L"The GitHub release contains an invalid download address.";
        return false;
    }

    try
    {
        const json manifest = json::parse(manifestText);
        if (!manifest.is_object() || manifest.value("schema", 0) != 1 ||
            manifest.value("platform", std::string{}) != "windows-x64" ||
            manifest.value("version", std::string{}) != expectedVersion)
        {
            error = L"The GitHub release manifest does not match this Windows build.";
            return false;
        }

        const json files = manifest.value("files", json::object());
        if (!files.is_object() || !files.contains(kExecutableName) || !files.contains(kTextureName))
        {
            error = L"The GitHub release manifest is missing required game files.";
            return false;
        }

        ReleasePackage parsed;
        parsed.version = std::string(expectedVersion);
        const auto parseFile = [&](const char *name, const std::string &url, ReleasePackageFile &file)
        {
            const json &entry = files.at(name);
            if (!entry.is_object())
                return false;
            file.sha256 = entry.value("sha256", std::string{});
            file.size = boundedSize(entry.value("size", json{}));
            file.url = url;
            return validHash(file.sha256) && file.size > 0;
        };

        if (!parseFile(kExecutableName, std::string(executableUrl), parsed.executable) ||
            !parseFile(kTextureName, std::string(textureUrl), parsed.texture))
        {
            error = L"The GitHub release manifest contains invalid file details.";
            return false;
        }

        package = std::move(parsed);
        error.clear();
        return true;
    }
    catch (const json::exception &)
    {
        error = L"The GitHub release manifest is not valid JSON.";
        return false;
    }
}
