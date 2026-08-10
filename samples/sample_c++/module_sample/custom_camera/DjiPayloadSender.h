#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>

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
    FILE *streamFile_;
};