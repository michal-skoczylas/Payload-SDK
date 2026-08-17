#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <cstdint>
#include <string>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/dict.h>
}

enum EncoderBackend
{
    ENCODER_AUTO = 0,   // h264_v4l2m2m gdy dostepny (RPi), inaczej libx264
    ENCODER_LIBX264 = 1,
    ENCODER_V4L2M2M = 2
};

class H264Encoder
{
public:
    H264Encoder(int width, int height, int fps, int bitrate,
                EncoderBackend backend = ENCODER_AUTO);
    ~H264Encoder();
    std::vector<uint8_t> encodedFrame(const cv::Mat &bgrFrame);
    bool isReady() const;
    std::string getEncoderName() const;

private:
    AVCodecContext *codecCtx_;
    AVFrame *frame_;
    SwsContext *swsCtx_;
    AVPacket *pkt_;
    int width_, height_, fps_, bitrate_;
    long long frameIndex_;
    bool ready_;
    std::string encoderName_;
};