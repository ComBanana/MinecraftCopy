#include "GameLauncher.h"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
std::wstring windowsErrorMessage(DWORD error)
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t *>(&buffer),
        0,
        nullptr);

    std::wstring message = length > 0 && buffer ? buffer : L"Windows could not start the game.";
    if (buffer)
        LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
        message.pop_back();
    return message;
}
}

VersionResult readGameVersion(const std::filesystem::path &gamePath)
{
    if (!std::filesystem::is_regular_file(gamePath))
        return {false, {}, L"The game executable is not installed."};

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0))
        return {false, {}, L"Could not check the installed game version."};
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring commandLine = L"\"" + gamePath.native() + L"\" --version";
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = writePipe;
    startupInfo.hStdError = writePipe;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(
        gamePath.c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        gamePath.parent_path().c_str(),
        &startupInfo,
        &processInfo);
    CloseHandle(writePipe);
    if (!created)
    {
        CloseHandle(readPipe);
        return {false, {}, L"Could not read the installed game version."};
    }

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 5000);
    if (waitResult == WAIT_TIMEOUT)
        TerminateProcess(processInfo.hProcess, ERROR_TIMEOUT);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    std::string output;
    char buffer[128];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
    {
        if (output.size() + bytesRead <= 128)
            output.append(buffer, bytesRead);
    }
    CloseHandle(readPipe);

    if (waitResult != WAIT_OBJECT_0 || exitCode != 0 || output.empty())
        return {false, {}, L"The installed game did not return a valid version."};

    while (!output.empty() && (output.back() == '\r' || output.back() == '\n' || output.back() == ' '))
        output.pop_back();
    if (output.empty() || output.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-+") != std::string::npos)
        return {false, {}, L"The installed game returned an invalid version."};

    return {true, std::wstring(output.begin(), output.end()), {}};
}

LaunchResult launchGame(const std::filesystem::path &gamePath, WorldMode mode)
{
    if (!std::filesystem::is_regular_file(gamePath))
    {
        return {false, L"MinecraftCopy.exe was not found beside the launcher. Build the game and launcher into the same output folder."};
    }

    const wchar_t *argument = mode == WorldMode::Finite ? L"--world=finite" : L"--world=infinite";
    std::wstring commandLine = L"\"" + gamePath.native() + L"\" " + argument;
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(
        gamePath.c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        gamePath.parent_path().c_str(),
        &startupInfo,
        &processInfo);

    if (!created)
        return {false, L"Could not start MinecraftCopy: " + windowsErrorMessage(GetLastError())};

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return {true, L"MinecraftCopy started."};
}
