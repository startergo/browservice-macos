#include "jpeg.hpp"

#include <iostream>

#include <turbojpeg.h>

static void check(
    bool condVal,
    const char* condStr,
    const char* condFile,
    int condLine
) {
    if(!condVal) {
        std::cerr << "FATAL ERROR " << condFile << ":" << condLine << ": ";
        std::cerr << "Condition '" << condStr << "' does not hold\n";
        abort();
    }
}

#define CHECK(condition) check((condition), #condition, __FILE__, __LINE__)

JPEGData compressJPEG(
    const uint8_t* image,
    size_t width,
    size_t height,
    size_t pitch,
    int quality
) {
    CHECK(width > 0 && height > 0);
    CHECK(quality >= 1 && quality <= 100);

    tjhandle handle = tjInitCompress();
    CHECK(handle != nullptr);

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    // TurboJPEG pitch is in bytes (row stride). Our data is BGRA with 4 bytes
    // per pixel. When pitch == width (tightly packed), pass 0 to use default.
    int pitchBytes;
    if(pitch == width) {
        pitchBytes = 0; // turbojpeg default: width * tjPixelSize[TJPF_BGRA]
    } else {
        pitchBytes = (int)(pitch * 4);
    }

    int result = tjCompress2(
        handle,
        image,
        (int)width,
        pitchBytes,
        (int)height,
        TJPF_BGRA,
        &jpegBuf,
        &jpegSize,
        TJSAMP_420,
        quality,
        0  // flags: default (no extra flags)
    );
    CHECK(result == 0);

    tjDestroy(handle);

    JPEGData jpegData;
    jpegData.data.reset(jpegBuf);
    jpegData.length = (size_t)jpegSize;

    return jpegData;
}
