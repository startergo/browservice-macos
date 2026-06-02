#pragma once

#include "common.hpp"

namespace retrojsvice {

class FileUpload;
class WebSocketConnection;

namespace http_ {
    class HTTPRequestHandler;
}

// State of single HTTP request response should be sent by calling one of
// send* functions exactly once If no response is given internal server
// error response is sent upon object destruction and warning is logged No
// other member functions may be called after sending response
class HTTPRequest {
SHARED_ONLY_CLASS(HTTPRequest);
private:
    class Impl;

public:
    // Private constructor
    HTTPRequest(CKey, unique_ptr<Impl> impl);

    string method();
    string path();
    string userAgent();

    // Form accessors return empty string/pointer if there is no entry with
    // specified name
    string getFormParam(string name);
    shared_ptr<FileUpload> getFormFile(string name);

    optional<string> getBasicAuthCredentials();

    // body function will be called to write body of response in a
    // different thread In case of HTTP server internal errors or server
    // shutdown body function may not be called or writing to given
    // ostream may throw exception Otherwise given content length
    // should match number of bytes written
    void sendResponse(
        int status,
        string contentType,
        uint64_t contentLength,
        function<void(ostream&)> body,
        bool noCache = true,
        vector<pair<string, string>> extraHeaders = {}
    );

    void sendTextResponse(
        int status,
        string text,
        bool noCache = true,
        vector<pair<string, string>> extraHeaders = {}
    );

    template <typename Data>
    void sendHTMLResponse(
        int status,
        void (*writer)(ostream&, const Data&),
        const Data& data,
        bool noCache = true,
        vector<pair<string, string>> extraHeaders = {}
    ) {
        stringstream htmlSS;
        writer(htmlSS, data);

        string html = htmlSS.str();
        uint64_t contentLength = html.size();

        sendResponse(
            status,
            "text/html; charset=UTF-8",
            contentLength,
            [html{move(html)}](ostream& out) {
                out << html;
            },
            noCache,
            move(extraHeaders)
        );
    }

private:
    unique_ptr<Impl> impl_;

    friend class http_::HTTPRequestHandler;
};

class SocketAddress {
public:
    // Create socket address from string representation of type ADDRESS - PORT
    // such as 127001 - 8080 Returns empty optional if parsing the
    // representation failed
    static optional<SocketAddress> parse(string repr);

private:
    SocketAddress();

    struct Impl;
    shared_ptr<Impl> impl_;

    friend class HTTPServer;
    friend ostream& operator<<(ostream& out, SocketAddress addr);
};

ostream& operator<<(ostream& out, SocketAddress addr);

class HTTPServerEventHandler {
public:
    virtual void onHTTPServerRequest(shared_ptr<HTTPRequest> request) = 0;
    virtual void onHTTPServerWebSocketConnection(
        shared_ptr<WebSocketConnection> connection,
        uint64_t windowHandle,
        string csrfToken
    ) = 0;
    virtual void onHTTPServerShutdownComplete() = 0;
};

// HTTP server that delegates requests to be handled by given event handler
// through onHTTPServerRequest Before destruction call shutdown and wait for
// onHTTPServerShutdownComplete event
class HTTPServer {
SHARED_ONLY_CLASS(HTTPServer);
public:
    HTTPServer(CKey,
        weak_ptr<HTTPServerEventHandler> eventHandler,
        SocketAddress listenAddr,
        int maxThreads
    );
    ~HTTPServer();

    void shutdown();

private:
    class Impl;
    shared_ptr<Impl> impl_;
};

}
