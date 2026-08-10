#pragma once
#include "IFrameSource.h"
#include <opencv2/opencv.hpp>
#include <string>

class FileFrameSource : public IFrameSource
{
public:
    FileFrameSource(std::string path);
    bool open();
    bool readFrame(cv::Mat &frame);
    void close();
    int getFps() const;
    int getWidth() const;
    int getHeight() const;

private:
    std::string path_;
    cv::VideoCapture cap_;
    int width_, height_, fps_;
    bool isOpen_;
};