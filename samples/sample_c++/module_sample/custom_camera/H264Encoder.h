#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}
class H264Encoder
{
public:
    H264Encoder(int width, int height, int fps, int bitrate);
    ~H264Encoder();
    std::vector<uint8_t> encodedFrame(const cv::Mat &bgrFrame);
    bool isReady() const;

private:
    AVCodecContext *codecCtx_;
    AVFrame *frame_;
    SwsContext *swsCtx_;
    AVPacket *pkt_;
    int width_, height_, fps_, bitrate_;
    long long frameIndex_;
    bool ready_;
};