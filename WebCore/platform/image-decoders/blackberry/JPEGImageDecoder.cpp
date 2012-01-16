/*
 * Copyright (C) QNX Software Systems 2010. All Rights Reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 */

/**
 * Implementation notes:
 *
 * Current implementation provides the source image size without doing a full
 * decode. It seems that libimg does not support partial decoding.
 */
#include "config.h"
#include "JPEGImageDecoder.h"

#include <errno.h>
#include <img/img.h>
#include <string.h>

namespace WebCore {

static img_lib_t s_ilib;
static bool      s_initialized = false;

static inline int libInit()
{
    if (s_initialized)
        return 0;

    if (img_lib_attach(&s_ilib) != IMG_ERR_OK) {
        LOG_ERROR("Unable to attach to libimg.");
        return -1;
    }
    s_initialized = true;
    return 0;
}

class ImageReader {
public:
    ImageReader(const char* data, size_t size) :
        m_data(data), m_size(size), m_width(0), m_height(0) { libInit(); }
    ~ImageReader() {}

    void updateData(const char* data, size_t size);
    int setSize(size_t width, size_t height);
    int sizeExtract(size_t& width, size_t& height);
    int decode(size_t width, size_t height, RGBA32Buffer* frame);

private:

    const char* m_data;
    size_t m_size;
    size_t m_width;
    size_t m_height;
};

// Function to get the original size of an image
static int imgDecodeSetup(_Uintptrt data, img_t* img, unsigned flags)
{
    ImageReader* reader = reinterpret_cast<ImageReader*>(data);

    if ((img->flags & (IMG_W | IMG_H)) == (IMG_W | IMG_H))
        reader->setSize(img->w, img->h);

    // Always: want to stop processing whether get a size or not.
    return IMG_ERR_INTR;
}

void ImageReader::updateData(const char* data, size_t size)
{
    m_data = data;
    m_size = size;
}

int ImageReader::setSize(size_t width, size_t height)
{
    m_width = width;
    m_height = height;
    return 0;
}

int ImageReader::sizeExtract(size_t& width, size_t& height)
{
    img_decode_callouts_t callouts;
    img_t img;
    io_stream_t* iostream = io_open(IO_MEM, IO_READ, m_size, m_data);
    if (!iostream)
        return -1;

    memset(&img, 0, sizeof(img));
    memset(&callouts, 0, sizeof(callouts));
    callouts.setup_f = imgDecodeSetup;
    callouts.data = reinterpret_cast<_Uintptrt>(this);

    int rc = img_load(s_ilib, iostream, &callouts, &img);
    io_close(iostream);
    if (rc != IMG_ERR_INTR)
        return -1;
    if (!m_width || !m_height)
        return -1;

    width = m_width;
    height = m_height;

    return 0;
}

int ImageReader::decode(size_t width, size_t height, RGBA32Buffer* frame)
{
    if (libInit() == -1)
        return -1;

    img_t img;
    memset(&img, 0, sizeof(img));
    img.format = IMG_FMT_RGB888;
    img.w = width;
    img.h = height;

    int stride = ((width * 3) + 3) & ~3;
    _uint8* buf = reinterpret_cast<_uint8*>(malloc(stride * height));
    if (!buf)
        return -1;
    img.access.direct.data = buf;
    img.access.direct.stride = stride;
    img.flags = IMG_W | IMG_H | IMG_DIRECT | IMG_FORMAT;

    io_stream_t* iostream = io_open(IO_MEM, IO_READ, m_size, m_data);
    if (!iostream) {
        free(buf);
        return -1;
    }

    int rc = img_load_resize(s_ilib, iostream, 0, &img);
    io_close(iostream);
    if (rc != IMG_ERR_OK) {
        free(buf);
        return -1;
    }

    for (unsigned j = 0; j < height; j++) {
        _uint8* curPtr = buf + j * stride;
        for (unsigned i = 0; i < width; i++) {
            frame->setRGBA(i, j, *(curPtr), *(curPtr + 1), *(curPtr + 2), 255);
            curPtr += 3;
        }
    }
    free(buf);
    return 0;
}

JPEGImageDecoder::JPEGImageDecoder(bool premultiplyAlpha)
    : ImageDecoder(premultiplyAlpha)
    , m_reader(0)
{
}

JPEGImageDecoder::~JPEGImageDecoder()
{
    delete m_reader;
}

void JPEGImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    ImageDecoder::setData(data, allDataReceived);

    if (m_reader)
        m_reader->updateData(m_data->data(), m_data->size());
}

bool JPEGImageDecoder::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable()) {
        if (!m_reader) {
            if (m_data)
                m_reader = new ImageReader(m_data->data(), m_data->size());
            if (!m_reader)
                return false;
        }
        size_t width, height;

        if (m_reader->sizeExtract(width, height) == -1)
            return false;
        if (!setSize(width, height))
            return false;
    }

    return ImageDecoder::isSizeAvailable();
}

RGBA32Buffer* JPEGImageDecoder::frameBufferAtIndex(size_t index)
{
    if (!isAllDataReceived())
        return 0;

    if (index)
        return 0;

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.resize(1);

    RGBA32Buffer& frame = m_frameBufferCache[0];

    // Check to see if it's already decoded. TODO: Could size change
    // between calls to this method?
    if (frame.status() == RGBA32Buffer::FrameComplete)
        return &frame;

    if (frame.status() == RGBA32Buffer::FrameEmpty) {
        if (!size().width() || !size().height()) {
            if (!isSizeAvailable())
                return 0;
        }
        if (!frame.setSize(size().width(), size().height())) {
            setFailed();
            return 0;
        }
        frame.setStatus(RGBA32Buffer::FramePartial);
        // For JPEGs, the frame always fills the entire image.
        frame.setRect(IntRect(IntPoint(), size()));
    }

    if (!m_reader) {
        setFailed();
        return 0;
    }

    if (m_reader->decode(size().width(), size().height(), &frame) == -1) {
        setFailed();
        return 0;
    }

    frame.setStatus(RGBA32Buffer::FrameComplete);
    return &frame;
}

}

