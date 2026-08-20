#include "DjiPayloadSender.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <iostream>

static const uint8_t kFrameAud[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x10};
static const size_t kChunkMaxLen = 60000;

DjiPayloadSender::DjiPayloadSender() : sentFrames_(0), sentBytes_(0), logStart_(std::chrono::steady_clock::now())
{
#ifdef DJI_STREAM_SIMULATE
    streamFile_ = fopen("stream_out.h264", "wb");
    if (streamFile_ == nullptr)
        std::cerr << "DjiPayloadSender: cannot open stream_out.h264" << std::endl;
#else
    streamFile_ = nullptr;
#endif
}

DjiPayloadSender::~DjiPayloadSender()
{
#ifdef DJI_STREAM_SIMULATE
    if (streamFile_)
        fclose(streamFile_);
#endif
}

bool DjiPayloadSender::send(const std::vector<uint8_t> &nalData)
{
    if (nalData.empty())
        return true;
    // zlozenie ramki
    std::vector<uint8_t> frame(nalData.size() + 6);
    memcpy(frame.data(), nalData.data(), nalData.size());
    memcpy(frame.data() + nalData.size(), kFrameAud, 6);
    // chunking ramki
    size_t offset = 0;
    while (offset < frame.size())
    {
        size_t chunk = std::min(kChunkMaxLen, frame.size() - offset);

#ifdef DJI_STREAM_SIMULATE
        if (fwrite(frame.data() + offset, 1, chunk, streamFile_) != chunk)
            return false;
#else
        if (!sendToDji(frame.data() + offset, chunk))
        {
            std::cerr << "SendVideoStream error, skipping remaining chunks"
                      << std::endl;
            return false;
        }
#endif
        offset += chunk;
    }

    // Rzeczywista wydajnosc (uzyteczna na dronie: porownaj z limitem 8 Mbps).
    sentFrames_++;
    sentBytes_ += frame.size();
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - logStart_).count();
    if (elapsed >= 5.0)
    {
        double mbps = (sentBytes_ * 8) / (elapsed * 1000.0 * 1000.0);
        double fps = sentFrames_ / elapsed;
        std::cout << "[SENDER] " << fps << " fps, " << mbps << " Mbps, "
                  << sentFrames_ << " frames" << std::endl;
        sentFrames_ = 0;
        sentBytes_ = 0;
        logStart_ = now;
    }
    return true;
}

#ifndef DJI_STREAM_SIMULATE
bool DjiPayloadSender::sendToDji(const uint8_t *data, size_t len)
{
    // Flow control: nie wysylaj gdy kanal wideo jest busy - DJI i tak odrzuci dane.
    T_DjiDataChannelState streamState;
    T_DjiReturnCode rcState = DjiPayloadCamera_GetVideoStreamState(&streamState);
    if (rcState == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS && streamState.busyState)
    {
        return false;
    }

    T_DjiReturnCode rc = DjiPayloadCamera_SendVideoStream(data, static_cast<uint16_t>(len));
    return rc == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}
#endif