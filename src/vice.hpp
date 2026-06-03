#pragma once

#include "timeout.hpp"

typedef struct VicePluginAPI_Context VicePluginAPI_Context;

namespace browservice {

// Vice plugin library for browser view and input relay
class VicePlugin {
SHARED_ONLY_CLASS(VicePlugin);
private:
    struct APIFuncs;

public:
    // Returns empty pointer if loading plugin failed
    static shared_ptr<VicePlugin> load(string filename);

    // Private constructor
    VicePlugin(CKey, CKey,
        string filename,
        void* lib,
        uint64_t apiVersion,
        unique_ptr<APIFuncs> apiFuncs
    );
    ~VicePlugin();

    string getVersionString();
    string getCreditsString();

    struct OptionDocsItem {
        string name;
        string valSpec;
        string desc;
        string defaultValStr;
    };
    vector<OptionDocsItem> getOptionDocs();

private:
    string filename_;
    void* lib_;
    uint64_t apiVersion_;

    unique_ptr<APIFuncs> apiFuncs_;

    friend class ViceContext;
};

class TempDir;

class ViceFileUpload {
SHARED_ONLY_CLASS(ViceFileUpload);
public:
    ViceFileUpload(CKey,
        shared_ptr<TempDir> tempDir,
        uint64_t uploadIdx,
        string name,
        PathStr srcPath,
        function<void()> srcCleanup
    );
    ~ViceFileUpload();

    PathStr path();

private:
    shared_ptr<TempDir> tempDir_;
    PathStr linkDir_;
    PathStr linkPath_;
    PathStr srcPath_;
    function<void()> srcCleanup_;
};

// Implementations of these event handlers may NOT call functions of ViceContext
// directly
class ViceContextEventHandler {
public:
    // Return 0 to deny window creation with msg
    virtual uint64_t onViceContextCreateWindowRequest(string& msg, optional<string> uri) = 0;

    virtual void onViceContextCloseWindow(uint64_t window) = 0;
    virtual void onViceContextResizeWindow(
        uint64_t window, int width, int height
    ) = 0;
    virtual void onViceContextFetchWindowImage(
        uint64_t window,
        function<void(const uint8_t*, size_t, size_t, size_t)> putImage
    ) = 0;

    virtual void onViceContextMouseDown(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onViceContextMouseUp(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onViceContextMouseMove(uint64_t window, int x, int y) = 0;
    virtual void onViceContextMouseDoubleClick(
        uint64_t window, int x, int y, int button
    ) = 0;
    virtual void onViceContextMouseWheel(
        uint64_t window, int x, int y, int dx, int dy
    ) = 0;
    virtual void onViceContextMouseLeave(uint64_t window, int x, int y) = 0;
    virtual void onViceContextKeyDown(uint64_t window, int key) = 0;
    virtual void onViceContextKeyUp(uint64_t window, int key) = 0;
    virtual void onViceContextLoseFocus(uint64_t window) = 0;

    virtual void onViceContextNavigate(uint64_t window, int direction) = 0;
    virtual void onViceContextNavigateToURI(uint64_t window, string uri) = 0;

    virtual void onViceContextCopyToClipboard(string text) = 0;
    virtual void onViceContextRequestClipboardContent() = 0;

    virtual void onViceContextUploadFile(
        uint64_t window, shared_ptr<ViceFileUpload> file
    ) = 0;
    virtual void onViceContextCancelFileUpload(uint64_t window) = 0;

    virtual string onViceContextWindowTitleQuery(uint64_t window) = 0;

    virtual void onViceContextZoomIn(uint64_t window) = 0;
    virtual void onViceContextZoomOut(uint64_t window) = 0;
    virtual void onViceContextZoomReset(uint64_t window) = 0;

    virtual void onViceContextShutdownComplete() = 0;
};

class CompletedDownload;

// initialized vice plugin context
class ViceContext : public enable_shared_from_this<ViceContext> {
SHARED_ONLY_CLASS(ViceContext);
public:
    // Init plugin - returns empty and logs on failure
    static shared_ptr<ViceContext> init(
        shared_ptr<VicePlugin> plugin,
        vector<pair<string, string>> options
    );

    // Private constructor
    ViceContext(CKey, CKey,
        shared_ptr<VicePlugin> plugin,
        VicePluginAPI_Context* handle
    );
    ~ViceContext();

    // Does plugin implement navigation controls (Data may be unavailable if plugin does
    // not support PluginNavigationControlSupportQuery plugin)
    optional<bool> hasNavigationControls();

    // Start context and retain event handler
    void start(shared_ptr<ViceContextEventHandler> eventHandler);
    void shutdown();

    // Vice plugin API functions for running contexts - 

    // Returns false with msg if denied
    bool requestCreatePopup(
        uint64_t parentWindow,
        uint64_t popupWindow,
        string& msg
    );
    void closeWindow(uint64_t window);

    void notifyWindowViewChanged(uint64_t window);

    void notifyWindowTitleChanged(uint64_t window);

    void notifyWindowNavigation(uint64_t window);

    void setWindowCursor(uint64_t window, int cursor);

    optional<pair<vector<string>, size_t>> windowQualitySelectorQuery(
        uint64_t window
    );
    void windowQualityChanged(uint64_t window, size_t idx);

    bool windowNeedsClipboardButtonQuery(uint64_t window);
    void windowClipboardButtonPressed(uint64_t window);
    void putClipboardContent(string text);

    void putFileDownload(uint64_t window, shared_ptr<CompletedDownload> file);

    bool startFileUpload(uint64_t window);
    void cancelFileUpload(uint64_t window);

private:
    static shared_ptr<ViceContext> getContext_(void* callbackData);

    void pumpEvents_();
    void shutdownComplete_();

    shared_ptr<VicePlugin> plugin_;
    VicePluginAPI_Context* ctx_;

    optional<bool> hasNavigationControls_;

    enum {Pending, Running, ShutdownComplete} state_;
    bool shutdownPending_;
    atomic<bool> pumpEventsInQueue_;
    atomic<bool> shutdownCompleteFlag_;

    shared_ptr<ViceContextEventHandler> eventHandler_;

    // For running contexts we store shared pointer to self to avoid it being
    // destructed
    shared_ptr<ViceContext> self_;

    void* callbackData_;

    struct WindowData {
        int width;
        int height;
    };
    uint64_t nextWindowHandle_;
    set<uint64_t> openWindows_;
    set<uint64_t> uploadModeWindows_;

    shared_ptr<TempDir> uploadTempDir_;
    uint64_t nextUploadIdx_;
};

}
