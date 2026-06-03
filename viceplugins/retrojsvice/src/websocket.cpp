#include "websocket.hpp"

#include "common.hpp"
#include "task_queue.hpp"

#include <Poco/Net/WebSocket.h>
#include <Poco/Base64Decoder.h>

#include <cstring>
#include <iostream>

namespace retrojsvice {

WebSocketConnection::WebSocketConnection(CKey,
    Poco::Net::HTTPServerRequest& request,
    Poco::Net::HTTPServerResponse& response,
    uint64_t windowHandle,
    string csrfToken,
    shared_ptr<TaskQueue> taskQueue
) :
    ws_(request, response),
    windowHandle_(windowHandle),
    csrfToken_(move(csrfToken)),
    taskQueue_(move(taskQueue)),
    closed_(false)
{
    // Extract HTTP Basic Auth credentials for validation by Context.
    // Must decode from Base64 to match the "user:password" form stored in
    // httpAuthCredentials_ and used by HTTPRequest::getBasicAuthCredentials().
    try {
        string scheme, authInfoBase64;
        if(request.hasCredentials()) {
            request.getCredentials(scheme, authInfoBase64);
            for(char& c : scheme) {
                c = tolower(c);
            }
            if(scheme == "basic") {
                stringstream ss(authInfoBase64);
                Poco::Base64Decoder decoder(ss);
                char buf[1024];
                while(decoder.good()) {
                    decoder.read(buf, sizeof(buf));
                    if(decoder.bad()) break;
                    basicAuthCredentials_.append(buf, (size_t)decoder.gcount());
                }
            }
        }
    } catch(const Poco::Exception&) {
        // No credentials or malformed header — leave empty
    }

    INFO_LOG("WebSocket connection opened for window ", windowHandle_);
}

WebSocketConnection::~WebSocketConnection() {
    INFO_LOG("WebSocket connection closed for window ", windowHandle_);
}

void WebSocketConnection::run() {
    readLoop_();
}

void WebSocketConnection::readLoop_() {
    try {
        constexpr int bufSize = 4096;
        char buf[bufSize];
        int flags;

        while(true) {
            int n = ws_.receiveFrame(buf, bufSize, flags);
            if(n == 0 || (flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) ==
                         Poco::Net::WebSocket::FRAME_OP_CLOSE) {
                break;
            }

            // Only handle text frames (client events/resize)
            if((flags & Poco::Net::WebSocket::FRAME_OP_BITMASK) ==
               Poco::Net::WebSocket::FRAME_OP_TEXT) {
                string msg(buf, (size_t)n);

                // Simple JSON-like parsing without a JSON library
                // Expected: {"type":"events","startIdx":42,"events":"MDN_100_200_0/"}
                //       or: {"type":"resize","width":1280,"height":720}

                if(msg.find("\"type\":\"events\"") != string::npos) {
                    // Extract startIdx
                    size_t idxPos = msg.find("\"startIdx\":");
                    size_t evtPos = msg.find("\"events\":");
                    if(idxPos != string::npos && evtPos != string::npos) {
                        size_t valStart = idxPos + 11; // len("\"startIdx\":")
                        size_t valEnd = msg.find(',', valStart);
                        if(valEnd == string::npos) valEnd = msg.find('}', valStart);
                        if(valEnd == string::npos) valEnd = msg.size();
                        string idxStr = msg.substr(valStart, valEnd - valStart);

                        // Extract events string (between quotes)
                        size_t evtQuoteStart = msg.find('"', evtPos + 10);
                        if(evtQuoteStart != string::npos) {
                            size_t evtValStart = evtQuoteStart + 1;
                            size_t evtValEnd = msg.find('"', evtValStart);
                            if(evtValEnd != string::npos && evtValEnd >= evtValStart) {
                                string evtStr = msg.substr(
                                    evtValStart, evtValEnd - evtValStart
                                );

                                optional<uint64_t> startIdx =
                                    parseString<uint64_t>(idxStr);
                                if(startIdx) {
                                    auto self = shared_from_this();
                                    postTask([self, idx{*startIdx},
                                              evtStr{move(evtStr)}]() mutable {
                                        REQUIRE_API_THREAD();
                                        if(self->closed_ || !self->eventsCallback_)
                                            return;
                                        self->eventsCallback_(idx, move(evtStr));
                                    });
                                }
                            }
                        }
                    }
                } else if(msg.find("\"type\":\"resize\"") != string::npos) {
                    size_t wPos = msg.find("\"width\":");
                    size_t hPos = msg.find("\"height\":");
                    if(wPos != string::npos && hPos != string::npos) {
                        size_t wValStart = wPos + 8;
                        size_t wValEnd = msg.find(',', wValStart);
                        if(wValEnd == string::npos) wValEnd = msg.find('}', wValStart);
                        string wStr = msg.substr(wValStart, wValEnd - wValStart);

                        size_t hValStart = hPos + 9;
                        size_t hValEnd = msg.find(',', hValStart);
                        if(hValEnd == string::npos) hValEnd = msg.find('}', hValStart);
                        string hStr = msg.substr(hValStart, hValEnd - hValStart);

                        optional<int> w = parseString<int>(wStr);
                        optional<int> h = parseString<int>(hStr);
                        if(w && h) {
                            auto self = shared_from_this();
                            postTask([self, width{*w}, height{*h}]() {
                                REQUIRE_API_THREAD();
                                if(self->closed_ || !self->resizeCallback_) return;
                                self->resizeCallback_(width, height);
                            });
                        }
                    }
                }
            }
        }
    } catch(const Poco::Exception& e) {
        if(!closed_) {
            INFO_LOG("WebSocket read loop ended for window ", windowHandle_,
                     ": ", e.displayText());
        }
    } catch(const exception& e) {
        if(!closed_) {
            INFO_LOG("WebSocket read loop ended for window ", windowHandle_,
                     ": ", e.what());
        }
    }

    // Send close frame to complete the WebSocket close handshake (RFC 6455 §5.5.1)
    try {
        ws_.shutdown();
    } catch(const Poco::Exception&) {
        // Already closed or connection reset — ignore
    }

    // Mark closed
    closed_ = true;
}

void WebSocketConnection::setCallbacks(
    function<void(int, int)> resizeCb,
    function<void(uint64_t, string)> eventsCb
) {
    resizeCallback_ = move(resizeCb);
    eventsCallback_ = move(eventsCb);
}

void WebSocketConnection::sendImage(
    const vector<uint8_t>& imageData,
    bool isJPEG,
    uint64_t imgIdx,
    int width,
    int height,
    int cursorSignal,
    bool iframeSignal
) {
    if(closed_) return;

    lock_guard<mutex> lock(sendMutex_);
    try {
        // Send metadata as text frame (JSON)
        string meta = string("{\"type\":\"frame\",\"idx\":") + toString(imgIdx) +
                      ",\"width\":" + toString(width) +
                      ",\"height\":" + toString(height) +
                      ",\"cursor\":" + toString(cursorSignal) +
                      ",\"iframe\":" + (iframeSignal ? "true" : "false") + "}";
        ws_.sendFrame(meta.data(), (int)meta.size(),
                      Poco::Net::WebSocket::FRAME_TEXT);

        // Send image data as binary frame
        ws_.sendFrame(imageData.data(), (int)imageData.size(),
                      Poco::Net::WebSocket::FRAME_BINARY);
    } catch(const Poco::Exception& e) {
        WARNING_LOG("WebSocket sendImage failed for window ", windowHandle_,
                    ": ", e.displayText());
        closed_ = true;
    }
}

void WebSocketConnection::sendIframeNotification() {
    if(closed_) return;

    lock_guard<mutex> lock(sendMutex_);
    try {
        string msg = "{\"type\":\"iframe\"}";
        ws_.sendFrame(msg.data(), (int)msg.size(),
                      Poco::Net::WebSocket::FRAME_TEXT);
    } catch(const Poco::Exception& e) {
        WARNING_LOG("WebSocket sendIframeNotification failed for window ",
                    windowHandle_, ": ", e.displayText());
        closed_ = true;
    }
}

void WebSocketConnection::sendScrollReset() {
    if(closed_) return;

    lock_guard<mutex> lock(sendMutex_);
    try {
        string msg = "{\"type\":\"scroll_reset\"}";
        ws_.sendFrame(msg.data(), (int)msg.size(),
                      Poco::Net::WebSocket::FRAME_TEXT);
    } catch(const Poco::Exception& e) {
        WARNING_LOG("WebSocket sendScrollReset failed for window ", windowHandle_,
                    ": ", e.displayText());
        closed_ = true;
    }
}

void WebSocketConnection::close() {
    if(closed_) return;
    closed_ = true;

    try {
        ws_.shutdown();
    } catch(const Poco::Exception& e) {
        // Shutdown may fail if already closed — ignore
    }
}

}
