#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
#include <string>

struct EncodedPacket
{
    uint8_t *data;
    size_t size;
};

class VideoEncoder
{
public:
    VideoEncoder(int width, int height, int fps);
    ~VideoEncoder();

    std::vector<uint8_t> encodeFrame(const cv::Mat &bgrFrame);

private:
    cv::VideoWriter writer_;
    std::string tempFilename_;
};