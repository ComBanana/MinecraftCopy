#pragma once

#include <Windows.h>
#include <WebView2.h>
#include <wrl.h>

#include "UpdateService.h"

#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <thread>

class LauncherApp
{
public:
    int run(HINSTANCE instance);

private:
    struct PendingUpdate
    {
        GameUpdateResult result;
    };

    static constexpr UINT kUpdateCompleteMessage = WM_APP + 1;
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool createBrowser();
    void startUpdateOperation(bool install);
    void sendUpdateState(const GameUpdateState &state);
    void showNativeError(const std::wstring &message) const;
    void reportPageMessage(const std::wstring &message);
    void handlePageMessage(ICoreWebView2WebMessageReceivedEventArgs *args);

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    std::wstring m_executableDirectory;
    std::unique_ptr<UpdateService> m_updateService;
    std::thread m_updateThread;
    std::mutex m_updateMutex;
    std::optional<PendingUpdate> m_pendingUpdate;
    bool m_updateBusy = false;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_environment;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;
    Microsoft::WRL::ComPtr<ICoreWebView2_3> m_webView3;
    EventRegistrationToken m_messageToken{};
    EventRegistrationToken m_navigationToken{};
    EventRegistrationToken m_newWindowToken{};
};
