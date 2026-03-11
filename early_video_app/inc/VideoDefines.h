/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef VIDEODEFINES_H_
#define VIDEODEFINES_H_

#include <list>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <chrono>

#include <sys/mman.h>   // declares mmap, mprotect, PROT_READ, PROT_WRITE, MAP_*
#include <fcntl.h>      // O_* flags if you open files
#include <unistd.h>     // close(), getpagesize(), etc.
#include <linux/videodev2.h>


namespace early_video_app {


/*Data structure define*/

/**
 * @brief:
 *  Base Class for Linear/Graphic Buffer
 */
enum Type {
    LINEAR = 0,
    GRAPHIC = 1,
    INVALID = 2,
};

class Buffer {
    public:
        Buffer(int32_t fd, uint64_t capacity, Type type) :
            mFd(fd),
            mCapacity(capacity),
            mType(type) {}

        explicit Buffer(int32_t fd, uint32_t dataOffset, uint64_t capacity, Type type) :
            mFd(fd),
            mDataOffset(dataOffset),
            mCapacity(capacity),
            mType(type) {}

        ~Buffer() {}

        enum Flags : uint32_t {
            NONE           = 0x0,
            EOS            = 0x1 << 0, //< End Of Stream
            CODEC_CONFIG   = 0x1 << 1, //< Buffer containing CSD data

            ERROR          = 0x1 << 30, //< Error frame. Data may be corrupt.
        };

        ///< Getter APIs for the Buffer Class
        bool isLinear() const {
            return mType == LINEAR;
        }

        bool isGraphic() const {
            return mType == GRAPHIC;
        }

        bool isValid() const {
            return mType != INVALID;
        }

        int32_t fd() {
            return mFd;
        }

        uint32_t width() {
            return mWidth;
        }

        uint32_t height() {
            return mHeight;
        }

        size_t capacity() {
            return mCapacity;
        }

        uint64_t offset() {
            return mOffset;
        }

        uint32_t flags() {
            return mFlags;
        }

        uint64_t timestamp() {
            return mTimeStamp;
        }

        uint64_t filledLength() {
            return mFilledLength;
        }

        uint64_t isLastSliceInFrame() {
            return mIsLastSliceInFrame;
        }

        uint64_t isLastSliceSkipped() {
            return mIsLastSliceSkipped;
        }

        uint32_t dataOffset() {
            return mDataOffset;
        }

        uint32_t layerId() {
            return mLayerId;
        }

        ///< Setter for the Buffer Class
        void setStrideAndScanline(uint32_t stride, uint32_t scanlines) {
            if (mType == GRAPHIC) {
                mStride = stride;
                mScanlines = scanlines;
            }
        }

        void setResolution(uint32_t width, uint32_t height) {
            mWidth = width;
            mHeight = height;
        }

        void setFilledRange(uint32_t offset, uint32_t filled) {
            mOffset = offset;
            mFilledLength = filled;
        }

        void setFlags(Flags _flags) {
            mFlags = _flags;
        }

        void setTimeStamp(uint64_t timestamp) {
            mTimeStamp = timestamp;
        }

        void setLastSliceInFrame(uint64_t lastsliceinframe) {
            mIsLastSliceInFrame = lastsliceinframe;
        }

        void setLastSliceSkipped(uint64_t lastsliceskipped) {
            mIsLastSliceSkipped = lastsliceskipped;
        }

        void setLayerId(uint32_t layerId) {
            mLayerId = layerId;
        }

        struct Mapping {
            Mapping(int fd, size_t size, uint32_t dataOffset)
                : length(static_cast<uint32_t>(size)) {
                    addr = reinterpret_cast<uint8_t *>(
                            mmap(NULL, size, PROT_READ | PROT_WRITE , MAP_SHARED, fd, dataOffset));
                }

            ~Mapping() {
                if (addr && length) {
                    munmap(addr, length);
                }
            }

            uint8_t * vaddr() {
                return addr;
            }

            uint32_t capacity() const {
                return length;
            }

            protected:
            uint8_t *addr;
            uint32_t length;
        };

        std::unique_ptr<Mapping> map() {
            return std::unique_ptr<Mapping>(
                    new Mapping(fd(), capacity(), dataOffset()));
        }

    private:
        int32_t  mFd = -1;

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        uint32_t mStride = 0;
        uint32_t mScanlines = 0;
        uint64_t mIsLastSliceInFrame = 0;

    // last queued slice in case of skip slices
        uint64_t mIsLastSliceSkipped = 0;

        uint64_t mOffset = 0;
        uint64_t mCapacity = 0;
        uint64_t mTimeStamp = 0;
        uint64_t mFilledLength = 0;

        uint32_t mDataOffset = 0;
        uint32_t mLayerId = 0;

        Type mType = Type::INVALID;
        Flags mFlags = Flags::NONE;
    };
}

class InputData {
    public:
        unsigned char* data = NULL;
        unsigned int length = 0;
        unsigned int frameIndex = 0;
        bool isCodecConfig = false;
        bool isLastFrame = false;
};

#endif /* VIDEODEFINES_H_ */