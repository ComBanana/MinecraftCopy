#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct ReleasePackageFile
{
    std::string sha256;
    std::uint64_t size = 0;
    std::string url;
};

struct ReleasePackage
{
    std::string version;
    ReleasePackageFile executable;
    ReleasePackageFile texture;
};

bool parseReleaseManifest(std::string_view manifestText,
                          std::string_view expectedVersion,
                          std::string_view executableUrl,
                          std::string_view textureUrl,
                          ReleasePackage &package,
                          std::wstring &error);
