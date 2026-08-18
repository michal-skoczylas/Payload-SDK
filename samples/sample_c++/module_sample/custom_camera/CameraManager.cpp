#include "CameraManager.h"
#include <iostream>
#include <cstdlib>

CameraManager::CameraManager(const std::string &videoPath) : videoPath_(videoPath)
{
    cap_.open(videoPath_);
    if (!cap_.isOpened())
    {
        std::cerr << "[ERROR] Couldnt open video file" << std::endl;
        exit(1);
    }

    int width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fps = static_cast<int>(cap_.get(cv::CAP_PROP_FPS));
    if (fps <= 0)
        fps = 30;

    std::cout << "cam init: " << width << "x" << height << " @" << fps << "fps\n";

    encoder_.reset(new VideoEncoder(width, height, fps));
}

CameraManager::~CameraManager()
{
    if (cap_.isOpened())
    {
        cap_.release();
    }
}

cv::Mat CameraManager::captureFrame()
{
    cv::Mat frame;
    cap_ >> frame;
    if (frame.empty())
    {
        cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
        cap_ >> frame;
    }
    return frame;
}

EncodedPacket CameraManager::processNextFrame()
{
    cv::Mat frame = captureFrame();

    if (!frame.empty() && encoder_)
    {
        currentH264Buffer_ = encoder_->encodeFrame(frame);
    }
    else
    {
        currentH264Buffer_.clear();
    }

    EncodedPacket packet;
    packet.data = currentH264Buffer_.empty() ? nullptr : currentH264Buffer_.data();
    packet.size = currentH264Buffer_.size();

    return packet;
}