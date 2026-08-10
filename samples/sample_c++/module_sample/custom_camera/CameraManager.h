#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <vector>
#include "VideoEncoder.h"

class CameraManager
{
public:
    CameraManager(const std::string &videoPath);
    ~CameraManager();

    cv::Mat captureFrame();
    EncodedPacket processNextFrame();

private:
    std::string videoPath_;
    cv::VideoCapture cap_;
    std::unique_ptr<VideoEncoder> encoder_;
    std::vector<uint8_t> currentH264Buffer_;
};