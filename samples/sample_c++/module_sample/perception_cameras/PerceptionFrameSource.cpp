#ifdef PERCEPTION_STREAM_ENABLED
#include "PerceptionFrameSource.h"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <iostream>

static PerceptionFrameSource *s_instance = nullptr;

PerceptionFrameSource::PerceptionFrameSource(E_DjiPerceptionDirection direction)
    : direction_(direction), leftDataType_(LeftDataTypeFor(direction)), width_(0), height_(0), fps_(20)
{
}

PerceptionFrameSource::~PerceptionFrameSource()
{
    close();
}

int PerceptionFrameSource::LeftDataTypeFor(E_DjiPerceptionDirection direction)
{
    switch (direction)
    {
    case DJI_PERCEPTION_RECTIFY_FRONT:
        return RECTIFY_FRONT_LEFT;
    case DJI_PERCEPTION_RECTIFY_REAR:
        return RECTIFY_REAR_LEFT;
    case DJI_PERCEPTION_RECTIFY_UP:
        return RECTIFY_UP_LEFT;
    case DJI_PERCEPTION_RECTIFY_LEFT:
        return RECTIFY_LEFT_LEFT;
    case DJI_PERCEPTION_RECTIFY_RIGHT:
        return RECTIFY_RIGHT_LEFT;
    case DJI_PERCEPTION_RECTIFY_DOWN:
    default:
        return RECTIFY_DOWN_LEFT;
    }
}

bool PerceptionFrameSource::open()
{
    T_DjiReturnCode returnCode;

    returnCode = DjiPerception_Init();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS)
    {
        std::cerr << "Perception init failed, error code: 0x" << std::hex << returnCode << std::endl;
        return false;
    }

    s_instance = this;
    returnCode = DjiPerception_SubscribePerceptionImage(direction_, &PerceptionFrameSource::ImageCallback);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS)
    {
        std::cerr << "Subscribe perception image failed, error code: 0x" << std::hex << returnCode << std::endl;
        DjiPerception_Deinit();
        s_instance = nullptr;
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    bool gotFirstFrame = cv_.wait_for(lock, std::chrono::seconds(1), [this]
                                      { return width_ > 0 && height_ > 0; });
    if (!gotFirstFrame)
    {
        std::cerr << "No perception image received within 1s" << std::endl;
        DjiPerception_UnsubscribePerceptionImage(direction_);
        DjiPerception_Deinit();
        s_instance = nullptr;
        return false;
    }

    isOpen_ = true;
    return true;
}

void PerceptionFrameSource::ImageCallback(T_DjiPerceptionImageInfo imageInfo, uint8_t *imageRawBuffer,
                                          uint32_t bufferLen)
{
    if (s_instance)
        s_instance->onImage(imageInfo, imageRawBuffer, bufferLen);
}

void PerceptionFrameSource::onImage(T_DjiPerceptionImageInfo imageInfo, uint8_t *imageRawBuffer, uint32_t bufferLen)
{
    if (imageInfo.dataType != (uint32_t)leftDataType_)
        return;

    if (imageRawBuffer == nullptr || bufferLen == 0)
        return;

    int width = (int)imageInfo.rawInfo.width;
    int height = (int)imageInfo.rawInfo.height;
    if (width % 2 != 0)
        width -= 1;
    if (height % 2 != 0)
        height -= 1;
    if (width <= 0 || height <= 0)
        return;

    size_t copyLen = std::min<size_t>(bufferLen, (size_t)width * (size_t)height);
    cv::Mat gray(height, width, CV_8U);
    memcpy(gray.data, imageRawBuffer, copyLen);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        latest_ = gray;
        newData_ = true;
        if (width_ == 0 || height_ == 0)
        {
            width_ = width;
            height_ = height;
        }
    }
    cv_.notify_all();
}

bool PerceptionFrameSource::readFrame(cv::Mat &frame)
{
    cv::Mat gray;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!newData_ || latest_.empty())
            return false;
        latest_.copyTo(gray);
        newData_ = false;
    }

    if (gray.channels() == 1)
        cv::cvtColor(gray, frame, cv::COLOR_GRAY2BGR);
    else
        gray.copyTo(frame);

    return true;
}

void PerceptionFrameSource::close()
{
    if (isOpen_)
    {
        DjiPerception_UnsubscribePerceptionImage(direction_);
        DjiPerception_Deinit();
        isOpen_ = false;
        s_instance = nullptr;
    }
}

int PerceptionFrameSource::getFps() const
{
    return fps_;
}

int PerceptionFrameSource::getWidth() const
{
    return width_;
}

int PerceptionFrameSource::getHeight() const
{
    return height_;
}

#endif // PERCEPTION_STREAM_ENABLED
