#include "H264Encoder.h"
#include <iostream>

H264Encoder::H264Encoder(int width, int height, int fps, int bitrate)
{
    std::cout << "header avcodec maj: " << LIBAVCODEC_VERSION_MAJOR << std::endl;
    std::cout << "runtime version:    " << av_version_info() << std::endl;
    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if (codec == nullptr)
    {
        std::cout << "nie znaleziono codeca libx264" << std::endl;

        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (codec == nullptr)
        {
            this->ready_ = false;
            std::cout << "Nie znaleziono codeca " << std::endl;
            return;
        }
    }

    codecCtx_ = avcodec_alloc_context3(codec);
    this->width_ = width;
    this->height_ = height;
    this->fps_ = fps;
    this->bitrate_ = bitrate;
    this->codecCtx_->width = this->width_;
    this->codecCtx_->height = this->height_;
    this->codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
    this->codecCtx_->time_base = {1, fps};
    this->codecCtx_->framerate = {fps, 1};
    this->codecCtx_->bit_rate = bitrate_;
    this->codecCtx_->gop_size = 2 * fps;
    this->codecCtx_->max_b_frames = 0;
    this->codecCtx_->thread_count = 1;
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "tune", "zerolatency", 0);
    if (!avcodec_open2(this->codecCtx_, codec, &opts))
    {
        av_dict_free(&opts);
        frame_ = av_frame_alloc();
        frame_->format = AV_PIX_FMT_YUV420P;
        frame_->width = width_;
        frame_->height = height_;
        av_frame_get_buffer(frame_, 0);
        swsCtx_ = sws_getContext(width_, height_, AV_PIX_FMT_BGR24,   // wejście: BGR z OpenCV
                                 width_, height_, AV_PIX_FMT_YUV420P, // wyjście: to, co chce enkoder
                                 SWS_BILINEAR, NULL, NULL, NULL);
        pkt_ = av_packet_alloc();
        if (frame_ && swsCtx_ && pkt_)
            ready_ = true;
        else
            ready_ = false;
    }
    else
    {
        av_dict_free(&opts);
        this->ready_ = false;
        return;
    }
}

std::vector<uint8_t> H264Encoder::encodedFrame(const cv::Mat &bgrFrame)
{
    if (!this->ready_ || bgrFrame.empty())
    {
        return {};
    }
    const uint8_t *srcData[4] = {};
    srcData[0] = (const uint8_t *)bgrFrame.data;
    srcData[1] = srcData[2] = srcData[3] = nullptr;

    int srcLinesize[4] = {};
    srcLinesize[0] = bgrFrame.cols * 3;
    srcLinesize[1] = srcLinesize[2] = srcLinesize[3] = 0;

    av_frame_make_writable(frame_);

    sws_scale(swsCtx_, srcData, srcLinesize, 0, height_, frame_->data, frame_->linesize);

    frame_->pts = frameIndex_++;

    if (avcodec_send_frame(codecCtx_, frame_) < 0)
    {
        std::cout << "send frame error" << std::endl;
        return {};
    }

    std::vector<uint8_t> out;
    while (avcodec_receive_packet(codecCtx_, pkt_) >= 0)
    {
        out.insert(out.end(), pkt_->data, pkt_->data + pkt_->size);
    }

    return out;
}

H264Encoder::~H264Encoder()
{
    av_packet_free(&pkt_);
    sws_freeContext(swsCtx_);
    av_frame_free(&frame_);
    avcodec_free_context(&codecCtx_);
}

bool H264Encoder::isReady() const
{
    return this->ready_;
}