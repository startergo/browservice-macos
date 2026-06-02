#pragma once

#include "common.hpp"

#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/WebSocket.h>

#include <atomic>

namespace retrojsvice {

class Window;
class TaskQueue;

// Manages a single WebSocket connection associated with a browser window.
// Receives events and resize notifications from the client, and pushes
// compressed image frames and control messages to the client.
class WebSocketConnection : public enable_shared_from_this<WebSocketConnection> {
    SHARED_ONLY_CLASS(WebSocketConnection);
public:
    WebSocketConnection(CKey,
        Poco::Net::HTTPServerRequest& request,
        Poco::Net::HTTPServerResponse& response,
        uint64_t windowHandle,
        string csrfToken,
        shared_ptr<TaskQueue> taskQueue
    );
    ~WebSocketConnection();

    // Start the background read loop. Should be called from the Poco handler
    // thread; this method blocks until the WebSocket closes.
    void run();

    // Set callbacks invoked on the API thread for incoming client messages.
    // Must be called before run().  Ordering is guaranteed: the
    // handleWebSocketConnection postTask fires before any readLoop_ message
    // tasks, so callbacks are always set by the time they are needed.
    void setCallbacks(
        function<void(int width, int height)> resizeCb,
        function<void(uint64_t startIdx, string events)> eventsCb
    );

    // Called from API thread to push a compressed image frame.
    // imageData contains raw JPEG or PNG bytes.
    void sendImage(
        const vector<uint8_t>& imageData,
        bool isJPEG,
        uint64_t imgIdx,
        int width,
        int height,
        int cursorSignal,
        bool iframeSignal
    );

    // Called from API thread to push an iframe notification.
    void sendIframeNotification();

    // Called from API thread to tell the client to reset its scrollbar to the top.
    void sendScrollReset();

    // Close the WebSocket connection.
    void close();

    uint64_t windowHandle() { return windowHandle_; }
    const string& csrfToken() { return csrfToken_; }

private:
    void readLoop_();

    Poco::Net::WebSocket ws_;
    uint64_t windowHandle_;
    string csrfToken_;
    shared_ptr<TaskQueue> taskQueue_;

    mutex sendMutex_;
    atomic<bool> closed_;

    // Set via setCallbacks(); called on the API thread from postTask lambdas.
    function<void(int, int)> resizeCallback_;
    function<void(uint64_t, string)> eventsCallback_;
};

}
