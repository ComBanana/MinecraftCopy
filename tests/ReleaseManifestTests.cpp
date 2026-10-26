#include "ReleaseManifest.h"

#include <iostream>
#include <string>

int main()
{
    const auto require = [](bool condition, const char *message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    };
    const auto replaceOnce = [](std::string text, const std::string &from, const std::string &to)
    {
        const std::size_t position = text.find(from);
        if (position != std::string::npos)
            text.replace(position, from.size(), to);
        return text;
    };

    const std::string executableHash(64, 'a');
    const std::string textureHash(64, 'b');
    const std::string manifest =
        "{\"schema\":1,\"platform\":\"windows-x64\",\"version\":\"0.0.1-alpha\",\"files\":{"
        "\"MinecraftCopy.exe\":{\"sha256\":\"" + executableHash + "\",\"size\":24680},"
        "\"textureatlas.png\":{\"sha256\":\"" + textureHash + "\",\"size\":321}}}";
    const std::string executableUrl =
        "https://github.com/ComBanana/MinecraftCopy/releases/download/v0.0.1-alpha/MinecraftCopy.exe";
    const std::string textureUrl =
        "https://github.com/ComBanana/MinecraftCopy/releases/download/v0.0.1-alpha/textureatlas.png";
    const auto rejected = [&](const std::string &candidate, const std::string &exeUrl)
    {
        ReleasePackage ignored;
        std::wstring error;
        return !parseReleaseManifest(candidate, "0.0.1-alpha", exeUrl, textureUrl, ignored, error);
    };

    bool passed = true;
    ReleasePackage package;
    std::wstring error;
    passed &= require(parseReleaseManifest(manifest, "0.0.1-alpha", executableUrl, textureUrl, package, error),
                      "a matching Windows manifest is accepted");
    passed &= require(package.version == "0.0.1-alpha" &&
                      package.executable.sha256 == executableHash && package.executable.size == 24680 &&
                      package.executable.url == executableUrl && package.texture.sha256 == textureHash &&
                      package.texture.size == 321 && package.texture.url == textureUrl,
                      "validated asset metadata is returned for installation");
    passed &= require(rejected(replaceOnce(manifest, "0.0.1-alpha", "0.0.2-alpha"), executableUrl),
                      "a manifest for a different release is rejected");
    passed &= require(rejected(replaceOnce(manifest, "windows-x64", "linux-x64"), executableUrl),
                      "a manifest for another platform is rejected");
    passed &= require(rejected(replaceOnce(manifest, executableHash, std::string(64, 'A')), executableUrl),
                      "uppercase or malformed SHA-256 values are rejected");
    passed &= require(rejected(replaceOnce(manifest, "24680", "0"), executableUrl),
                      "zero-byte assets are rejected");
    passed &= require(rejected(replaceOnce(manifest, "24680", "536870913"), executableUrl),
                      "assets larger than the download limit are rejected");
    passed &= require(rejected(replaceOnce(manifest, "\"textureatlas.png\":", "\"other.png\":"), executableUrl),
                      "a manifest missing the texture atlas is rejected");
    passed &= require(rejected("{not json", executableUrl), "malformed JSON is rejected");
    passed &= require(rejected(manifest, "https://example.com/MinecraftCopy.exe"),
                      "assets hosted outside the official release path are rejected");
    return passed ? 0 : 1;
}
