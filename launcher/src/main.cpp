#include "LauncherApp.h"

#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult))
    {
        MessageBoxW(nullptr, L"The launcher could not initialize Windows COM services.", L"MinecraftCopy Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }

    LauncherApp app;
    const int result = app.run(instance);
    CoUninitialize();
    return result;
}
