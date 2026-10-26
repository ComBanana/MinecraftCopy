#include "LauncherApp.h"

#include "BuildVersion.h"
#include "GameLauncher.h"

#include <Shlwapi.h>
#include <wrl.h>
#include <json.hpp>

#include <filesystem>
#include <string>
#include <vector>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace
{
constexpr wchar_t kWindowClass[] = L"MinecraftCopyLauncherWindow";
constexpr wchar_t kWindowTitle[] = L"MinecraftCopy Launcher";
constexpr wchar_t kVirtualHost[] = L"minecraftcopy.local";
constexpr wchar_t kLocalOrigin[] = L"https://minecraftcopy.local/";

std::wstring executableDirectory()
{
    std::vector<wchar_t> buffer(512);
    while (true)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size() - 1)
            return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path().native();
        buffer.resize(buffer.size() * 2);
    }
}

std::string utf8(const std::wstring &value)
{
    if (value.empty())
        return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0)
        return {};
    std::string output(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), output.data(), length, nullptr, nullptr);
    return output;
}
}

int LauncherApp::run(HINSTANCE instance)
{
    m_instance = instance;
    m_executableDirectory = executableDirectory();
    if (m_executableDirectory.empty())
    {
        showNativeError(L"The launcher could not locate its own application folder.");
        return 1;
    }
    m_updateService = std::make_unique<UpdateService>(std::filesystem::path(m_executableDirectory));

    LPWSTR browserVersion = nullptr;
    const HRESULT runtimeResult = GetAvailableCoreWebView2BrowserVersionString(nullptr, &browserVersion);
    if (browserVersion)
        CoTaskMemFree(browserVersion);
    if (FAILED(runtimeResult))
    {
        showNativeError(L"Microsoft Edge WebView2 Runtime is required to use the launcher. Install the Evergreen WebView2 Runtime, then try again.");
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = m_instance;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        showNativeError(L"The launcher window could not be created.");
        return 1;
    }

    RECT bounds{0, 0, 1280, 800};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    m_window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        m_instance,
        this);
    if (!m_window)
    {
        showNativeError(L"The launcher window could not be created.");
        return 1;
    }

    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);
    if (!createBrowser())
    {
        DestroyWindow(m_window);
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

bool LauncherApp::createBrowser()
{
    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT environmentResult, ICoreWebView2Environment *environment) -> HRESULT
            {
                if (FAILED(environmentResult) || !environment)
                {
                    showNativeError(L"WebView2 could not initialize the launcher interface.");
                    if (m_window)
                        DestroyWindow(m_window);
                    return S_OK;
                }

                m_environment = environment;
                return m_environment->CreateCoreWebView2Controller(
                    m_window,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT controllerResult, ICoreWebView2Controller *controller) -> HRESULT
                        {
                            if (FAILED(controllerResult) || !controller)
                            {
                                showNativeError(L"WebView2 could not create the launcher window content.");
                                if (m_window)
                                    DestroyWindow(m_window);
                                return S_OK;
                            }

                            m_controller = controller;
                            if (FAILED(m_controller->get_CoreWebView2(&m_webView)) || !m_webView)
                            {
                                showNativeError(L"WebView2 did not provide a browser view.");
                                DestroyWindow(m_window);
                                return S_OK;
                            }
                            if (FAILED(m_webView.As(&m_webView3)) || !m_webView3)
                            {
                                showNativeError(L"The installed WebView2 Runtime does not support local launcher content.");
                                DestroyWindow(m_window);
                                return S_OK;
                            }

                            RECT clientBounds{};
                            GetClientRect(m_window, &clientBounds);
                            m_controller->put_Bounds(clientBounds);
                            m_controller->put_IsVisible(TRUE);

                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(m_webView->get_Settings(&settings)) && settings)
                            {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_AreDevToolsEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->put_IsZoomControlEnabled(FALSE);
                            }

                            m_webView->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT
                                    {
                                        handlePageMessage(args);
                                        return S_OK;
                                    }).Get(),
                                &m_messageToken);

                            m_webView->add_NavigationStarting(
                                Callback<ICoreWebView2NavigationStartingEventHandler>(
                                    [](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT
                                    {
                                        LPWSTR uri = nullptr;
                                        if (SUCCEEDED(args->get_Uri(&uri)) && uri)
                                        {
                                            const std::wstring address(uri);
                                            const std::wstring origin(kLocalOrigin);
                                            args->put_Cancel(address.rfind(origin, 0) != 0);
                                            CoTaskMemFree(uri);
                                        }
                                        else
                                        {
                                            args->put_Cancel(TRUE);
                                        }
                                        return S_OK;
                                    }).Get(),
                                &m_navigationToken);

                            m_webView->add_NewWindowRequested(
                                Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                    [](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT
                                    {
                                        args->put_Handled(TRUE);
                                        return S_OK;
                                    }).Get(),
                                &m_newWindowToken);

                            const std::filesystem::path uiDirectory = std::filesystem::path(m_executableDirectory) / L"launcher" / L"ui";
                            const HRESULT mappingResult = m_webView3->SetVirtualHostNameToFolderMapping(
                                kVirtualHost,
                                uiDirectory.c_str(),
                                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
                            if (FAILED(mappingResult))
                            {
                                showNativeError(L"The launcher could not load its local interface files.");
                                DestroyWindow(m_window);
                                return S_OK;
                            }
                            const HRESULT navigateResult = m_webView->Navigate(L"https://minecraftcopy.local/index.html");
                            if (FAILED(navigateResult))
                            {
                                showNativeError(L"The launcher could not open its local interface.");
                                DestroyWindow(m_window);
                            }
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(result))
    {
        showNativeError(L"WebView2 could not be started.");
        return false;
    }
    return true;
}

void LauncherApp::handlePageMessage(ICoreWebView2WebMessageReceivedEventArgs *args)
{
    if (!args)
        return;

    LPWSTR source = nullptr;
    if (FAILED(args->get_Source(&source)) || !source)
        return;
    const std::wstring sourceAddress(source);
    CoTaskMemFree(source);
    if (sourceAddress.rfind(kLocalOrigin, 0) != 0)
        return;

    LPWSTR message = nullptr;
    if (FAILED(args->TryGetWebMessageAsString(&message)) || !message)
        return;
    const std::wstring action(message);
    CoTaskMemFree(message);

    if (action == L"app:ready")
    {
        reportPageMessage(L"launcher-version:" + std::wstring(MINECRAFTCOPY_VERSION, MINECRAFTCOPY_VERSION + std::char_traits<char>::length(MINECRAFTCOPY_VERSION)));
        startUpdateOperation(false);
        return;
    }
    if (action == L"updates:check")
    {
        startUpdateOperation(false);
        return;
    }
    if (action == L"updates:install")
    {
        startUpdateOperation(true);
        return;
    }

    WorldMode mode;
    if (action == L"play:finite")
        mode = WorldMode::Finite;
    else if (action == L"play:infinite")
        mode = WorldMode::Infinite;
    else
        return;

    if (m_updateBusy)
    {
        reportPageMessage(L"Wait for the game update to finish before launching.");
        return;
    }
    const std::filesystem::path executable = m_updateService->gameExecutable();
    if (executable.empty())
    {
        reportPageMessage(L"Install the game before launching it.");
        return;
    }
    const LaunchResult result = launchGame(executable, mode);
    reportPageMessage(result.message);
}

void LauncherApp::startUpdateOperation(bool install)
{
    if (m_updateBusy || !m_updateService)
        return;
    m_updateBusy = true;
    reportPageMessage(install ? L"Downloading and verifying the game…" : L"Checking GitHub for game updates…");
    try
    {
        m_updateThread = std::thread([this, install]
        {
            GameUpdateResult result;
            try
            {
                if (install)
                {
                    result = m_updateService->installLatest();
                }
                else
                {
                    result.state = m_updateService->checkForUpdates();
                    result.success = result.state.updateCheckSucceeded;
                }
            }
            catch (const std::exception &)
            {
                result.state.message = L"The game update could not be completed.";
            }
            {
                std::lock_guard<std::mutex> lock(m_updateMutex);
                m_pendingUpdate = PendingUpdate{std::move(result)};
            }
            PostMessageW(m_window, kUpdateCompleteMessage, 0, 0);
        });
    }
    catch (const std::system_error &)
    {
        m_updateBusy = false;
        reportPageMessage(L"Could not start the update check.");
    }
}

void LauncherApp::sendUpdateState(const GameUpdateState &state)
{
    if (!m_webView)
        return;
    const nlohmann::json message{
        {"type", "update-state"},
        {"gameInstalled", state.gameInstalled},
        {"installedVersion", utf8(state.installedVersion)},
        {"latestVersion", utf8(state.latestVersion)},
        {"updateCheckSucceeded", state.updateCheckSucceeded},
        {"updateAvailable", state.updateAvailable},
        {"message", utf8(state.message)}
    };
    const std::string serialized = message.dump();
    const std::wstring wideSerialized(serialized.begin(), serialized.end());
    m_webView->PostWebMessageAsJson(wideSerialized.c_str());
}

void LauncherApp::reportPageMessage(const std::wstring &message)
{
    if (m_webView)
        m_webView->PostWebMessageAsString(message.c_str());
}

void LauncherApp::showNativeError(const std::wstring &message) const
{
    MessageBoxW(m_window, message.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
}

LRESULT CALLBACK LauncherApp::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LauncherApp *app = reinterpret_cast<LauncherApp *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *createInfo = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        app = static_cast<LauncherApp *>(createInfo->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->m_window = window;
    }
    return app ? app->handleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT LauncherApp::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case kUpdateCompleteMessage:
    {
        if (m_updateThread.joinable())
            m_updateThread.join();
        std::optional<PendingUpdate> pending;
        {
            std::lock_guard<std::mutex> lock(m_updateMutex);
            pending = std::move(m_pendingUpdate);
            m_pendingUpdate.reset();
        }
        m_updateBusy = false;
        if (pending)
            sendUpdateState(pending->result.state);
        return 0;
    }
    case WM_SIZE:
        if (m_controller)
        {
            RECT bounds{};
            GetClientRect(m_window, &bounds);
            m_controller->put_Bounds(bounds);
        }
        return 0;
    case WM_DESTROY:
        if (m_updateThread.joinable())
            m_updateThread.join();
        if (m_webView)
        {
            m_webView->remove_WebMessageReceived(m_messageToken);
            m_webView->remove_NavigationStarting(m_navigationToken);
            m_webView->remove_NewWindowRequested(m_newWindowToken);
        }
        if (m_controller)
            m_controller->Close();
        m_webView.Reset();
        m_webView3.Reset();
        m_controller.Reset();
        m_environment.Reset();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(m_window, message, wParam, lParam);
    }
}
