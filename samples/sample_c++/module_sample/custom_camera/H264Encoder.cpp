#include "H264Encoder.h"
#include <iostream>

H264Encoder::H264Encoder(int width, int height, int fps, int bitrate, EncoderBackend backend)
{
    ready_ = false;
    width_ = width;
    height_ = height;
    fps_ = fps;
    bitrate_ = bitrate;
    frameIndex_ = 0;
    codecCtx_ = nullptr;
    frame_ = nullptr;
    swsCtx_ = nullptr;
    pkt_ = nullptr;

    std::cout << "header avcodec maj: " << LIBAVCODEC_VERSION_MAJOR << std::endl;
    std::cout << "runtime version:    " << av_version_info() << std::endl;

    // Kolejka kandydatow zalezna od wybranego backendu.
    // AUTO: najpierw sprzet (h264_v4l2m2m - RPi), potem software (libx264).
    std::vector<const char *> candidates;
    switch (backend)
    {
    case ENCODER_V4L2M2M:
        candidates.push_back("h264_v4l2m2m");
        break;
    case ENCODER_LIBX264:
        candidates.push_back("libx264");
        break;
    default:
        candidates.push_back("h264_v4l2m2m");
        candidates.push_back("libx264");
        break;
    }

    for (size_t i = 0; i < candidates.size(); ++i)
    {
        const bool isX264 = (std::string(candidates[i]) == "libx264");
        const AVCodec *codec = avcodec_find_encoder_by_name(candidates[i]);

        // Ostatnia deska ratunku dla x264: dowolny enkoder H.264 z systemu
        if (codec == nullptr && isX264)
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (codec == nullptr)
        {
            std::cout << "nie znaleziono enkodera: " << candidates[i] << std::endl;
            continue;
        }

        codecCtx_ = avcodec_alloc_context3(codec);
        codecCtx_->width = width_;
        codecCtx_->height = height_;
        codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
        codecCtx_->time_base = {1, fps_};
        codecCtx_->framerate = {fps_, 1};
        codecCtx_->bit_rate = bitrate_;
        codecCtx_->gop_size = 2 * fps_;
        codecCtx_->max_b_frames = 0;
        codecCtx_->thread_count = 0; // x264: wszystkie rdzenie; v4l2m2m ignoruje

        AVDictionary *opts = nullptr;
        if (isX264)
        {
            av_dict_set(&opts, "tune", "zerolatency", 0);
            av_dict_set(&opts, "preset", "superfast", 0); // medium = ~312 ms/klatke na ARM, za wolne na 30 fps
        }
        // v4l2m2m: zadnych opcji x264-owych - avcodec_open2 by je odrzucil

        if (avcodec_open2(codecCtx_, codec, &opts) == 0)
        {
            av_dict_free(&opts);
            encoderName_ = codec->name;
            break; // sukces - ten enkoder zostaje
        }

        av_dict_free(&opts);
        std::cerr << "avcodec_open2 nie powiodlo sie dla: " << candidates[i] << std::endl;
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }

    if (codecCtx_ == nullptr)
    {
        std::cerr << "zaden enkoder H.264 nie jest dostepny" << std::endl;
        return;
    }

    std::cout << "encoder: " << encoderName_ << std::endl;

    const AVPixelFormat pixFmt = (codecCtx_->pix_fmt != AV_PIX_FMT_NONE)
                                     ? codecCtx_->pix_fmt
                                     : AV_PIX_FMT_YUV420P;

    frame_ = av_frame_alloc();
    frame_->format = pixFmt;
    frame_->width = width_;
    frame_->height = height_;
    av_frame_get_buffer(frame_, 0);
    swsCtx_ = sws_getContext(width_, height_, AV_PIX_FMT_BGR24,   // wejście: BGR z OpenCV
                             width_, height_, pixFmt,             // wyjście: czego chce enkoder
                             SWS_BILINEAR, NULL, NULL, NULL);
    pkt_ = av_packet_alloc();
    if (frame_ && swsCtx_ && pkt_)
        ready_ = true;
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

std::string H264Encoder::getEncoderName() const
{
    return encoderName_;
}