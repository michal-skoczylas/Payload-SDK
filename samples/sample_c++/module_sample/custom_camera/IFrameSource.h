#pragma once
#include <opencv2/opencv.hpp>
class IFrameSource
{

public:
    virtual bool open() = 0;
    virtual bool readFrame(cv::Mat &frame) = 0;
    virtual bool readDepth(cv::Mat &depth)
    {
        (void)depth;
        return false;
    }
    virtual void close() = 0;
    virtual int getFps() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual ~IFrameSource() = default;
};