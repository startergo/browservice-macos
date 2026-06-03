#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <turbojpeg.h>

struct JPEGData {
    struct Free {
        void operator()(uint8_t* data) {
            if(data != nullptr) {
                tjFree(data);
            }
        }
    };

    std::unique_ptr<uint8_t[], Free> data;
    size_t length;
};

// Compress given image into JPEG image data should be in format where
// for all 0 <= y < height and 0 <= x < width image[4 * (y * pitch + x) + c]
// is value for color blue green and red for c = 0 1 2 respectively
// Quality should be in range 1-100
JPEGData compressJPEG(
    const uint8_t* image,
    size_t width,
    size_t height,
    size_t pitch,
    int quality = 80
);
