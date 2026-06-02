#pragma once

#include "common.hpp"

class PNGCompressor;

namespace retrojsvice {

class ImageCompressorEventHandler {
public:
    // handler must call func exactly once with image specs before
    // returning image is specified using argument set
    // (image width height pitch) where width > 0 and height > 0 For all
    // 0 <= y < height and 0 <= x < width image[4 * (y * pitch + x) + c] is the
    // value for color blue green and red for c = 0 1 2 respectively The
    // callback func will not retain image pointer - it will copy data
    // before returning
    virtual void onImageCompressorFetchImage(
        function<void(const uint8_t*, size_t, size_t, size_t)> func
    ) = 0;

    virtual void onImageCompressorRenderGUI(
        vector<uint8_t>& data, size_t width, size_t height
    ) = 0;
};

class DelayedTaskTag;
class HTTPRequest;

// Image compressor service for single browser window image pipeline is
// run asynchronously - when updated image is available service is
// notified by calling updateNotify() - when it is ready to begin compressing it
// it uses onImageCompressorFetchImage event handler to fetch most
// recent image At most one image is being compressed at time in separate
// background thread At most one HTTP request is kept waiting for new image
// to complete at time - previous requests are responded to upon each
// sendCompressedImage* call
class ImageCompressor : public enable_shared_from_this<ImageCompressor> {
SHARED_ONLY_CLASS(ImageCompressor);
public:
    ImageCompressor(CKey,
        weak_ptr<ImageCompressorEventHandler> eventHandler,
        steady_clock::duration sendTimeout,
        int quality
    );
    ~ImageCompressor();

    // Supported values - 10100 for JPEG and 101 for PNG
    int quality();
    void setQuality(MCE, int quality);

    void updateNotify(MCE);

    // Send most recent compressed image immediately
    void sendCompressedImageNow(MCE, shared_ptr<HTTPRequest> httpRequest);

    // Send image once new compressed image is available or timeout
    // sendTimeout (given in constructor) is reached
    void sendCompressedImageWait(MCE, shared_ptr<HTTPRequest> httpRequest);

    // Make sure that compressor will never call onImageCompressorFetchImage
    // again (effectively stopping compressor from starting to compress new
    // images)
    void stopFetching();

    // Flush possible pending sendCompressedImageWait request with latest
    // image available immediately
    void flush(MCE);

    // Enable push mode: instead of waiting for sendCompressedImageWait calls,
    // the compressor invokes the callback whenever a new compressed image is
    // ready. Used for WebSocket streaming.
    // imageData: raw JPEG or PNG bytes
    // isJPEG: true for JPEG, false for PNG
    void setPushMode(MCE,
        function<void(const vector<uint8_t>& imageData, bool isJPEG)> callback
    );

    // Functions for changing signal propagated in size of the
    // compressed image By default both are 1
    static constexpr int IframeSignalTrue = 0;
    static constexpr int IframeSignalFalse = 1;
    static constexpr int IframeSignalCount = 2;

    void setIframeSignal(MCE, int signal);

    static constexpr int CursorSignalHand = 0;
    static constexpr int CursorSignalNormal = 1;
    static constexpr int CursorSignalText = 2;
    static constexpr int CursorSignalCount = 3;

    void setCursorSignal(MCE, int signal);

private:
    void afterConstruct_(shared_ptr<ImageCompressor> self);

    // A CompressedImage is a callable that writes the image data to an HTTP
    // response. In push mode, the callback wraps the data differently.
    typedef function<void(shared_ptr<HTTPRequest>)> CompressedImage;

    tuple<vector<uint8_t>, size_t, size_t> fetchImage_(MCE);

    void pump_(MCE);
    void compressTaskDone_(
        MCE,
        CompressedImage compressedImage,
        shared_ptr<vector<uint8_t>> rawData,
        bool isJPEG
    );

    weak_ptr<ImageCompressorEventHandler> eventHandler_;
    steady_clock::duration sendTimeout_;
    int quality_;

    int iframeSignal_;
    int cursorSignal_;

    shared_ptr<PNGCompressor> pngCompressor_;

    thread compressorThread_;
    mutex compressorMutex_;
    condition_variable compressorCv_;
    bool compressorShutdownScheduled_;
    bool compressorTaskScheduled_;
    function<void()> compressorTask_;

    shared_ptr<DelayedTaskTag> waitTag_;
    CompressedImage compressedImage_;

    bool fetchingStopped_;
    bool imageUpdated_;
    bool compressedImageUpdated_;
    bool compressionInProgress_;

    // Push mode (WebSocket streaming)
    bool pushMode_;
    function<void(const vector<uint8_t>&, bool)> pushCallback_;
};

}
