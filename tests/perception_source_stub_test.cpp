#include "perception_cameras/PerceptionFrameSource.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

extern "C" {
int g_initCalls = 0;
int g_deinitCalls = 0;
int g_subCalls = 0;
int g_unsubCalls = 0;

T_DjiReturnCode DjiPerception_Init(void)
{
    g_initCalls++;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode DjiPerception_Deinit(void)
{
    g_deinitCalls++;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode DjiPerception_SubscribePerceptionImage(E_DjiPerceptionDirection direction,
                                                       DjiPerceptionImageCallback callback)
{
    g_subCalls++;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode DjiPerception_UnsubscribePerceptionImage(E_DjiPerceptionDirection direction)
{
    g_unsubCalls++;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}
}

static T_DjiPerceptionImageInfo MakeInfo(uint32_t dataType, uint32_t w, uint32_t h)
{
    T_DjiPerceptionImageInfo info;
    memset(&info, 0, sizeof(info));
    info.dataType = dataType;
    info.rawInfo.width = w;
    info.rawInfo.height = h;
    info.rawInfo.bpp = 8;
    return info;
}

int main()
{
    // 1. open() bez ramek -> timeout (1s) -> false + cleanup
    {
        PerceptionFrameSource src(DJI_PERCEPTION_RECTIFY_DOWN);
        assert(!src.open());
        assert(g_initCalls == 1 && g_subCalls == 1);
        assert(g_unsubCalls == 1 && g_deinitCalls == 1);
    }

    // 2. open() z ramka dostarczona w watku (jak watek SDK) -> sukces + clamp parzystych wymiarow
    {
        PerceptionFrameSource src(DJI_PERCEPTION_RECTIFY_DOWN);

        T_DjiPerceptionImageInfo leftInfo = MakeInfo(RECTIFY_DOWN_LEFT, 641, 481);
        std::vector<uint8_t> buf(641u * 481u, 128);

        std::thread feed([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            PerceptionFrameSource::ImageCallback(leftInfo, buf.data(), (uint32_t)buf.size());
        });

        assert(src.open());
        feed.join();

        assert(src.getWidth() == 640);
        assert(src.getHeight() == 480);
        assert(src.getFps() == 20);

        // 3. readFrame -> BGR 640x480
        cv::Mat f;
        assert(src.readFrame(f));
        assert(f.channels() == 3);
        assert(f.cols == 640 && f.rows == 480);

        // 4. brak nowej ramki -> false
        cv::Mat f2;
        assert(!src.readFrame(f2));

        // 5. ramka z PRAWEJ kamery -> odrzucona (dalej brak danych)
        T_DjiPerceptionImageInfo rightInfo = MakeInfo(RECTIFY_DOWN_RIGHT, 640, 480);
        std::vector<uint8_t> buf2(640u * 480u, 64);
        PerceptionFrameSource::ImageCallback(rightInfo, buf2.data(), (uint32_t)buf2.size());
        assert(!src.readFrame(f2));

        // 6. nowa ramka z LEWEJ -> true
        PerceptionFrameSource::ImageCallback(leftInfo, buf.data(), (uint32_t)buf.size());
        assert(src.readFrame(f2));

        // 7. close -> unsubscribe + deinit
        src.close();
        assert(g_unsubCalls == 2 && g_deinitCalls == 2);
    }

    std::cout << "[PASS] perception stub test" << std::endl;
    return 0;
}
