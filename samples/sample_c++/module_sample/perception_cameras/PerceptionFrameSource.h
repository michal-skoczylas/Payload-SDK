#pragma once
#include "custom_camera/IFrameSource.h"
#include <dji_perception.h>
#include <opencv2/opencv.hpp>
#include <mutex>
#include <condition_variable>
#include <atomic>

class PerceptionFrameSource : public IFrameSource
{
public:
    explicit PerceptionFrameSource(E_DjiPerceptionDirection direction = DJI_PERCEPTION_RECTIFY_DOWN);
    ~PerceptionFrameSource();

    bool open() override;
    bool readFrame(cv::Mat &frame) override;
    void close() override;
    int getFps() const override;
    int getWidth() const override;
    int getHeight() const override;

    static void ImageCallback(T_DjiPerceptionImageInfo imageInfo, uint8_t *imageRawBuffer, uint32_t bufferLen);

private:
    void onImage(T_DjiPerceptionImageInfo imageInfo, uint8_t *imageRawBuffer, uint32_t bufferLen);
    static int LeftDataTypeFor(E_DjiPerceptionDirection direction);

    E_DjiPerceptionDirection direction_;
    int leftDataType_;
    std::mutex mutex_;
    std::condition_variable cv_;
    cv::Mat latest_;
    std::atomic<bool> newData_{false};
    std::atomic<bool> isOpen_{false};
    int width_;
    int height_;
    int fps_;
};
