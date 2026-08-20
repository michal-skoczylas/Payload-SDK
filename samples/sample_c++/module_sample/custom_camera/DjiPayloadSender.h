#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>
#include <chrono>

// nie trzeba includeowac jak bedzie symulacja
#ifndef DJI_STREAM_SIMULATE
#include <dji_payload_camera.h>
#endif
class DjiPayloadSender
{
public:
    DjiPayloadSender();
    ~DjiPayloadSender();
    bool send(const std::vector<uint8_t> &nalData);

private:
    bool sendToDji(const uint8_t *data, size_t len);

    FILE *streamFile_;
    size_t sentFrames_;
    size_t sentBytes_;
    std::chrono::steady_clock::time_point logStart_;
};