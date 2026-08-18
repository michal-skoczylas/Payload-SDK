#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cstdlib>

#include "RealsenseFrameSource.h"
#include "H264Encoder.h"
#include "DjiPayloadSender.h"
#include "VideoStreamPipeline.h"

// End-to-end offline: RealSense -> H264Encoder -> DjiPayloadSender(symulacja)
// Buduj z -DDJI_STREAM_SIMULATE, wynik w stream_out.h264
// Uzycie: realsense_pipeline_test [width height fps bitrate]
int main(int argc, char **argv)
{
    try
    {
        const int width = argc > 1 ? atoi(argv[1]) : 1280;
        const int height = argc > 2 ? atoi(argv[2]) : 720;
        const int fps = argc > 3 ? atoi(argv[3]) : 30;
        const int bitrate = argc > 4 ? atoi(argv[4]) : 4000000;

        auto source = std::unique_ptr<IFrameSource>(new RealsenseFrameSource(width, height, fps));
        if (!source->open())
        {
            std::cerr << "[FAIL] Could not open RealSense camera" << std::endl;
            return 1;
        }
        std::cout << "Opened: " << source->getWidth() << "x" << source->getHeight()
                  << " @ " << source->getFps() << " fps" << std::endl;

        auto encoder = std::unique_ptr<H264Encoder>(
            new H264Encoder(source->getWidth(), source->getHeight(), source->getFps(), bitrate));
        if (!encoder->isReady())
        {
            std::cerr << "[FAIL] Encoder failed to be ready" << std::endl;
            return 1;
        }

        auto sender = std::unique_ptr<DjiPayloadSender>(new DjiPayloadSender());

        VideoStreamPipeline pipeline(std::move(source), std::move(encoder), std::move(sender));
        if (!pipeline.start())
        {
            std::cerr << "[FAIL] Pipeline failed to start" << std::endl;
            return 1;
        }

        std::cout << "Pipeline running for ~10s..." << std::endl;
        for (int i = 0; i < 100; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (i % 25 == 0)
                std::cout << "  " << i / 10 << "s" << std::endl;
        }

        pipeline.stop();
        std::cout << "[DONE] stream_out.h264 written" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "err: " << e.what() << std::endl;
        return 1;
    }
}
