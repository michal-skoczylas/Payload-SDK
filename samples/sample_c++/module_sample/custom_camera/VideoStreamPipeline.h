#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include "IFrameSource.h"

class H264Encoder;
class DjiPayloadSender;

class VideoStreamPipeline
{
public:
    VideoStreamPipeline(std::unique_ptr<IFrameSource> source,
                        std::unique_ptr<H264Encoder> encoder,
                        std::unique_ptr<DjiPayloadSender> sender);
    ~VideoStreamPipeline(); // musi wołać stop
    bool start();           // odpala wątek
    void stop();            // flaga + join()
    bool isRunning() const;

private:
    void run(); // pętla wątku
    std::unique_ptr<IFrameSource> source_;
    std::unique_ptr<H264Encoder> encoder_;
    std::unique_ptr<DjiPayloadSender> sender_;
    std::thread thread_;
    std::atomic<bool> running_;
};