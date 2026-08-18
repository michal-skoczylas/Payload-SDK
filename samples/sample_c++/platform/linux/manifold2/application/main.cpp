/**
 ********************************************************************
 * @file    main.cpp
 * @brief
 *
 * @copyright (c) 2021 DJI. All rights reserved.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI’s authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <liveview/test_liveview_entry.hpp>
#include <perception/test_perception_entry.hpp>
#include <flight_control/test_flight_control.h>
#include <gimbal/test_gimbal_entry.hpp>
#include "application.hpp"
#include "fc_subscription/test_fc_subscription.h"
#include <gimbal_emu/test_payload_gimbal_emu.h>
#include <camera_emu/test_payload_cam_emu_media.h>
#include <camera_emu/test_payload_cam_emu_base.h>
#include <dji_logger.h>
#include "widget/test_widget.h"
#include "widget/test_widget_speaker.h"
#include <power_management/test_power_management.h>
#include "data_transmission/test_data_transmission.h"
#include <flight_controller/test_flight_controller_entry.h>
#include <positioning/test_positioning.h>
#include <hms_manager/hms_manager_entry.h>
#include "camera_manager/test_camera_manager_entry.h"

// custom lib
#include "custom_camera/FileFrameSource.h"
#include "custom_camera/H264Encoder.h"
#include "custom_camera/DjiPayloadSender.h"
#include "custom_camera/VideoStreamPipeline.h"
#ifdef REALSENSE_INSTALLED
#include "custom_camera/RealsenseFrameSource.h"
#endif
#include <memory>
#include <thread>
#include <chrono>

/* Private constants ---------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private values -------------------------------------------------------------*/

/* Private functions declaration ---------------------------------------------*/
void DjiUser_RunCustomVideoStreamSample();
#ifdef REALSENSE_INSTALLED
void DjiUser_RunCustomRealsenseStreamSample();
#endif

/* Exported functions definition ---------------------------------------------*/
int main(int argc, char **argv)
{
    Application application(argc, argv);
    char inputChar;
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiReturnCode returnCode;
    T_DjiTestApplyHighPowerHandler applyHighPowerHandler;

start:
    std::cout
        << "\n"
        << "| Available commands:                                                                              |\n"
        << "| [0] Fc subscribe sample - subscribe quaternion and gps data                                      |\n"
        << "| [1] Flight controller sample - you can control flying by PSDK                                    |\n"
        << "| [2] Hms info manager sample - get health manger system info by language                          |\n"
        << "| [a] Gimbal manager sample - you can control gimbal by PSDK                                       |\n"
        << "| [c] Camera stream view sample - display the camera video stream                                  |\n"
        << "| [d] Stereo vision view sample - display the stereo image                                         |\n"
        << "| [e] Run camera manager sample - you can test camera's functions interactively                    |\n"
        << "| [f] Start rtk positioning sample - you can receive rtk rtcm data when rtk signal is ok           |\n"
        << "| [v] Custom video stream sample - stream local video file to drone                              |\n"
#ifdef REALSENSE_INSTALLED
        << "| [r] RealSense stream sample - stream RealSense camera (preset select)                          |\n"
#endif
        << std::endl;

    std::cin >> inputChar;
    switch (inputChar) {
        case '0':
            DjiTest_FcSubscriptionRunSample();
            break;
        case '1':
            DjiUser_RunFlightControllerSample();
            break;
        case '2':
            DjiUser_RunHmsManagerSample();
            break;
        case 'a':
            DjiUser_RunGimbalManagerSample();
            break;
        case 'c':
            DjiUser_RunCameraStreamViewSample();
            break;
        case 'd':
            DjiUser_RunStereoVisionViewSample();
            break;
        case 'e':
            DjiUser_RunCameraManagerSample();
            break;
        case 'f':
            returnCode = DjiTest_PositioningStartService();
            if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
                USER_LOG_ERROR("rtk positioning sample init error");
                break;
            }

            USER_LOG_INFO("Start rtk positioning sample successfully");
            break;
        case 'v':
            DjiUser_RunCustomVideoStreamSample();
            break;
        case 'r':
#ifdef REALSENSE_INSTALLED
            DjiUser_RunCustomRealsenseStreamSample();
#else
            std::cout << "RealSense not available in this build" << std::endl;
#endif
            break;
        default:
            break;
    }

    osalHandler->TaskSleepMs(2000);

    goto start;
}

/* Private functions definition-----------------------------------------------*/

void DjiUser_RunCustomVideoStreamSample()
{
    USER_LOG_INFO("Starting test video stream");
    try
    {
        const std::string videoPath = "drone_vid.mp4";

        auto source = std::unique_ptr<IFrameSource>(new FileFrameSource(videoPath));
        if (!source->open())
        {
            USER_LOG_ERROR("Could not open video: %s", videoPath.c_str());
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

        USER_LOG_INFO("Starting sending video...");
        if (!pipeline.start())
        {
            USER_LOG_ERROR("Pipeline failed to start");
            return;
        }

        // 30 fps, wysylka leci w watku pipeline
        for (int i = 0; i < 300; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (i % 10 == 0)
            {
                std::cout << "[STREAM] running, " << i / 10 << "s" << std::endl;
            }
        }

        pipeline.stop();
        USER_LOG_INFO("Stream ended");
    }
    catch (const std::exception &e)
    {
        USER_LOG_ERROR("Error occured in stream %s", e.what());
    }
}

#ifdef REALSENSE_INSTALLED
void DjiUser_RunCustomRealsenseStreamSample()
{
    USER_LOG_INFO("Starting RealSense video stream");

    int preset = 1;
    std::cout << "Select RealSense preset:\n"
              << "  [1] 1280x720 @ 30 (default)\n"
              << "  [2] 1920x1080 @ 30\n"
              << "  [3] 640x480 @ 60\n"
              << "> ";
    std::cin >> preset;

    int width, height, fps, bitrate;
    switch (preset)
    {
    case 2:
        width = 1920; height = 1080; fps = 30; bitrate = 6000000;
        break;
    case 3:
        width = 640; height = 480; fps = 60; bitrate = 3000000;
        break;
    default:
        width = 1280; height = 720; fps = 30; bitrate = 4000000;
        break;
    }

    try
    {
        auto source = std::unique_ptr<IFrameSource>(new RealsenseFrameSource(width, height, fps));
        if (!source->open())
        {
            USER_LOG_ERROR("Could not open RealSense camera");
            return;
        }

        auto encoder = std::unique_ptr<H264Encoder>(
            new H264Encoder(source->getWidth(), source->getHeight(), source->getFps(), bitrate));
        if (!encoder->isReady())
        {
            USER_LOG_ERROR("Encoder failed to be ready");
            return;
        }

        auto sender = std::unique_ptr<DjiPayloadSender>(new DjiPayloadSender());

        VideoStreamPipeline pipeline(std::move(source), std::move(encoder), std::move(sender));

        USER_LOG_INFO("Starting sending video...");
        if (!pipeline.start())
        {
            USER_LOG_ERROR("Pipeline failed to start");
            return;
        }

        // 30 fps, wysylka leci w watku pipeline
        for (int i = 0; i < 300; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (i % 10 == 0)
            {
                std::cout << "[STREAM] running, " << i / 10 << "s" << std::endl;
            }
        }

        pipeline.stop();
        USER_LOG_INFO("Stream ended");
    }
    catch (const std::exception &e)
    {
        USER_LOG_ERROR("Error occured in stream %s", e.what());
    }
}
#endif

/****************** (C) COPYRIGHT DJI Innovations *****END OF FILE****/
