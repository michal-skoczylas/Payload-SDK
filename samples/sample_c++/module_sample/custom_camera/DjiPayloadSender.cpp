#include "DjiPayloadSender.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <iostream>

static const uint8_t kFrameAud[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x10};
static const size_t kChunkMaxLen = 60000;

DjiPayloadSender::DjiPayloadSender()
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
        T_DjiReturnCode rc = DjiPayloadCamera_SendVideoStream(frame.data() + offset, chunk);

        if (rc != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS)
        {
            std::cerr << "SendVideoStream error: 0x" << std::hex << rc << std::endl;
            return false;
        }
#endif
        offset += chunk;
    }
    return true;
}