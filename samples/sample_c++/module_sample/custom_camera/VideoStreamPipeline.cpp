#include "VideoStreamPipeline.h"
#include "H264Encoder.h"
#include "DjiPayloadSender.h"
#include <iostream>
#include <chrono>

VideoStreamPipeline::VideoStreamPipeline(std::unique_ptr<IFrameSource> source,
                                         std::unique_ptr<H264Encoder> encoder,
                                         std::unique_ptr<DjiPayloadSender> sender)
    : source_(std::move(source)), encoder_(std::move(encoder)), sender_(std::move(sender)),
      running_(false)
{
}

VideoStreamPipeline::~VideoStreamPipeline()
{
    stop();
}

bool VideoStreamPipeline::start()
{
    if (running_)
        return false;
    running_ = true;
    thread_ = std::thread(&VideoStreamPipeline::run, this);
    return true;
}

void VideoStreamPipeline::stop()
{
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

bool VideoStreamPipeline::isRunning() const
{
    return running_;
}

void VideoStreamPipeline::run()
{
    const double period = 1.0 / source_->getFps();
    using clock = std::chrono::steady_clock;
    auto next = clock::now();

    while (running_)
    {
        cv::Mat frame;
        if (!source_->readFrame(frame))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        std::vector<uint8_t> nals = encoder_->encodedFrame(frame);
        if (!sender_->send(nals))
            std::cerr << "[PIPELINE] send failed" << std::endl;

        next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(period));
        if (next > clock::now())
            std::this_thread::sleep_until(next);
        else
            next = clock::now();
    }
}
