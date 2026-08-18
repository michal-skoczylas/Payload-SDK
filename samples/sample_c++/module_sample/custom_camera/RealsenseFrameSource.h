#pragma once
#include "IFrameSource.h"
#include <opencv2/opencv.hpp>
// if realsense lib is installed
// #ifdef REALSENSE_INSTALLED
#include <librealsense2/rs.hpp>
class RealsenseFrameSource : public IFrameSource
{
public:
    RealsenseFrameSource(int width, int height, int fps);
    ~RealsenseFrameSource();

    bool open() override;
    bool readFrame(cv::Mat &frame) override;
    bool readDepth(cv::Mat &depth) override;

    void close() override;
    int getFps() const override;
    int getWidth() const override;
    int getHeight() const override;

private:
    rs2::pipeline pipe_;
    rs2::config cfg_;
    int width_;
    int height_;
    int fps_;
    bool isOpen_;
    cv::Mat depth_;
};

// #endif
