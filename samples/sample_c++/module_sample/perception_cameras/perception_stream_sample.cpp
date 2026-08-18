#ifdef PERCEPTION_STREAM_ENABLED
#include "perception_stream_sample.h"
#include "PerceptionFrameSource.h"
#include "custom_camera/H264Encoder.h"
#include "custom_camera/DjiPayloadSender.h"
#include "custom_camera/VideoStreamPipeline.h"
#include <dji_logger.h>
#include <dji_perception.h>
#include <memory>
#include <thread>
#include <chrono>
#include <iostream>

void DjiUser_RunPerceptionStreamSample()
{
    USER_LOG_INFO("Starting perception camera stream");

    try
    {
        auto source = std::unique_ptr<IFrameSource>(
            new PerceptionFrameSource(DJI_PERCEPTION_RECTIFY_DOWN));
        if (!source->open())
        {
            USER_LOG_ERROR("Could not open perception camera");
            return;
        }

        auto encoder = std::unique_ptr<H264Encoder>(
            new H264Encoder(source->getWidth(), source->getHeight(), source->getFps(), 4000000));
        if (!encoder->isReady())
        {
            USER_LOG_ERROR("Encoder failed to be ready");
            return;
        }

        auto sender = std::unique_ptr<DjiPayloadSender>(new DjiPayloadSender());

        VideoStreamPipeline pipeline(std::move(source), std::move(encoder), std::move(sender));

        USER_LOG_INFO("Starting sending perception video...");
        if (!pipeline.start())
        {
            USER_LOG_ERROR("Pipeline failed to start");
            return;
        }

        for (int i = 0; i < 300; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (i % 10 == 0)
            {
                std::cout << "[PERCEPTION-STREAM] running, " << i / 10 << "s" << std::endl;
            }
        }

        pipeline.stop();
        USER_LOG_INFO("Perception stream ended");
    }
    catch (const std::exception &e)
    {
        USER_LOG_ERROR("Error occured in perception stream %s", e.what());
    }
}

#endif // PERCEPTION_STREAM_ENABLED
