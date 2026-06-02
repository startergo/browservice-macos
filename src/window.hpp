#pragma once

#include "browser_area.hpp"
#include "control_bar.hpp"
#include "download_manager.hpp"
#include "image_slice.hpp"
#include "root_widget.hpp"

class CefBrowser;
class CefFileDialogCallback;
class CefRequestContext;

namespace browservice {

class Window;

class WindowEventHandler {
public:
    virtual void onWindowClose(uint64_t handle) = 0;
    virtual void onWindowCleanupComplete(uint64_t handle) = 0;
    virtual void onWindowViewImageChanged(uint64_t handle) = 0;
    virtual void onWindowTitleChanged(uint64_t handle) = 0;
    virtual void onWindowNavigationStarted(uint64_t handle) = 0;
    virtual void onWindowCursorChanged(uint64_t handle, int cursor) = 0;
    virtual optional<pair<vector<string>, size_t>> onWindowQualitySelectorQuery(
        uint64_t handle
    ) = 0;
    virtual void onWindowQualityChanged(uint64_t handle, size_t idx) = 0;
    virtual bool onWindowNeedsClipboardButtonQuery(uint64_t handle) = 0;
    virtual void onWindowClipboardButtonPressed(uint64_t handle) = 0;
    virtual void onWindowDownloadCompleted(
        uint64_t handle, shared_ptr<CompletedDownload> file
    ) = 0;
    virtual bool onWindowStartFileUpload(uint64_t handle) = 0;
    virtual void onWindowCancelFileUpload(uint64_t handle) = 0;

    // Accept popup creation returning new window
    virtual void onWindowCreatePopupRequest(
        uint64_t handle,
        function<shared_ptr<Window>(uint64_t)> accept
    ) = 0;
};

class Timeout;
class ViceFileUpload;

// Lifecycle - 1 Open 2 Closed 3 CleanupComplete
class Window :
    public WidgetParent,
    public ControlBarEventHandler,
    public BrowserAreaEventHandler,
    public DownloadManagerEventHandler,
    public enable_shared_from_this<Window>
{
SHARED_ONLY_CLASS(Window);
public:
    // Returns empty pointer if CEF browser creation fails
    static shared_ptr<Window> tryCreate(
        shared_ptr<WindowEventHandler> eventHandler,
        CefRefPtr<CefRequestContext> requestContext,
        uint64_t handle,
        optional<string> uri,
        bool showSoftNavigationButtons
    );

    // Private constructor
    Window(CKey, CKey);

    // Before destruction ensure that cleanup is complete
    ~Window();

    void close();
    void resize(int width, int height);
    ImageSlice fetchViewImage();

    string fetchTitle();

    // -1 = back 0 = refresh 1 = forward
    void navigate(int direction);

    void navigateToURI(string uri);

    void uploadFile(shared_ptr<ViceFileUpload> file);
    void cancelFileUpload();

    // Pass and sanitize input events
    void sendMouseDownEvent(int x, int y, int button);
    void sendMouseUpEvent(int x, int y, int button);
    void sendMouseMoveEvent(int x, int y);
    void sendMouseDoubleClickEvent(int x, int y, int button);
    void sendMouseWheelEvent(int x, int y, int dx, int dy);
    void sendMouseLeaveEvent(int x, int y);
    void sendKeyDownEvent(int key);
    void sendKeyUpEvent(int key);
    void sendLoseFocusEvent();

    void zoomIn();
    void zoomOut();
    void zoomReset();

    // WidgetParent - 
    virtual void onWidgetViewDirty() override;
    virtual void onWidgetCursorChanged() override;
    virtual void onGlobalHotkeyPressed(GlobalHotkey key) override;

    // ControlBarEventHandler - 
    virtual void onAddressSubmitted(string url) override;
    virtual void onQualityChanged(size_t idx) override;
    virtual void onPendingDownloadAccepted() override;
    virtual void onFind(string text, bool forward, bool findNext) override;
    virtual void onStopFind(bool clearSelection) override;
    virtual void onClipboardButtonPressed() override;
    virtual void onOpenBookmarksButtonPressed() override;
    virtual void onNavigationButtonPressed(int direction) override;
    virtual void onHomeButtonPressed() override;

    // BrowserAreaEventHandler - 
    virtual void onBrowserAreaViewDirty() override;

    // DownloadManagerEventHandler - 
    virtual void onPendingDownloadCountChanged(int count) override;
    virtual void onDownloadProgressChanged(vector<int> progress) override;
    virtual void onDownloadCompleted(
        shared_ptr<CompletedDownload> file
    ) override;

private:
    // Class that implements CefClient interfaces for window
    class Client;

    // To create a window - tryCreate init_ create CEF browser
    void init_(shared_ptr<WindowEventHandler> eventHandler, CefRefPtr<CefRequestContext> requestContext, uint64_t handle, bool showSoftNavigationButtons);
    void createSuccessful_();
    void createFailed_();

    void afterClose_();

    void watchdog_();
    void updateSecurityStatus_();
    void updateZoom_();

    void clampMouseCoords_(int& x, int& y);

    // May call onWindowViewImageChanged immediately
    void signalImageChanged_();

    // May call onWindowTitleChanged immediately
    void signalTitleChanged_();

    uint64_t handle_;
    enum {Open, Closed, CleanupComplete} state_;

    // Empty only in CleanupComplete state
    shared_ptr<WindowEventHandler> eventHandler_;

    CefRefPtr<CefRequestContext> requestContext_;

    bool showSoftNavigationButtons_;

    bool imageChanged_;

    bool titleChanged_;
    string title_;

    // Empty only in CleanupComplete state
    CefRefPtr<CefBrowser> browser_;

    ImageSlice rootViewport_;
    shared_ptr<RootWidget> rootWidget_;

    shared_ptr<DownloadManager> downloadManager_;

    shared_ptr<Timeout> watchdogTimeout_;

    // window is in file upload mode when fileUploadCallback_ is nonempty
    CefRefPtr<CefFileDialogCallback> fileUploadCallback_;
    vector<shared_ptr<ViceFileUpload>> retainedUploads_;

    double zoomLevel_;
};

}
