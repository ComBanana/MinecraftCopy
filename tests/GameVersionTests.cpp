#include "BuildVersion.h"
#include "GameLauncher.h"

#include <iostream>
#include <string>

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2)
    {
        std::wcerr << L"Expected the built game executable path.\n";
        return 1;
    }

    const VersionResult result = readGameVersion(argv[1]);
    if (!result.success)
    {
        std::wcerr << L"Version detection failed: " << result.error << L'\n';
        return 1;
    }

    const std::string expectedBytes = MINECRAFTCOPY_VERSION;
    const std::wstring expected(expectedBytes.begin(), expectedBytes.end());
    if (result.version != expected)
    {
        std::wcerr << L"Expected version " << expected << L", detected " << result.version << L".\n";
        return 1;
    }

    std::wcout << L"Detected game version " << result.version << L".\n";
    return 0;
}
