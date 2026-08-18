#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "H264Encoder.h"
#include "FileFrameSource.h"
#include "DjiPayloadSender.h"
#include "VideoStreamPipeline.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_path>" << std::endl;
        return 2;
    }
    const std::string videoPath = argv[1];

    auto source = std::unique_ptr<IFrameSource>(new FileFrameSource(videoPath));
    if (!source->open())
    {
        std::cerr << "[FAIL] Could not open: " << videoPath << std::endl;
        return 1;
    }
    std::cout << "Opened: " << source->getWidth() << "x" << source->getHeight()
              << " @ " << source->getFps() << " fps" << std::endl;

    auto encoder = std::unique_ptr<H264Encoder>(new H264Encoder(source->getWidth(), source->getHeight(),
                                                                source->getFps(), 4000000));
    if (!encoder->isReady())
    {
        std::cerr << "Encoder failed to be ready" << std::endl;
        return 1;
    }

    auto sender = std::unique_ptr<DjiPayloadSender>(new DjiPayloadSender());

    VideoStreamPipeline pipeline(std::move(source), std::move(encoder), std::move(sender));

    if (!pipeline.start())
    {
        std::cerr << "[FAIL] Could not start pipeline" << std::endl;
        return 1;
    }
    std::cout << "Pipeline started, running 3s..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));

    pipeline.stop();
    std::cout << "[PASS] Pipeline ran and stopped cleanly" << std::endl;
    return 0;
}
