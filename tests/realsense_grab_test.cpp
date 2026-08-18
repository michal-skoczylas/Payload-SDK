#include <iostream>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>

#include "RealsenseFrameSource.h"

int main()
{
    RealsenseFrameSource source(1280, 720, 30);

    // macOS potrafi zawiesic open() ("failed to set power state") - retry jak w pipeline
    bool opened = false;
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        if (source.open())
        {
            opened = true;
            break;
        }
        std::cerr << "[WARN] open() attempt " << attempt + 1 << " failed, retrying..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (!opened)
    {
        std::cerr << "[FAIL] Could not open RealSense camera" << std::endl;
        return 1;
    }

    std::cout << "Opened: " << source.getWidth() << "x" << source.getHeight()
              << " @ " << source.getFps() << " fps" << std::endl;

    const int framesToRead = 300;   // ~10 s przy 30 fps
    const int maxConsecutiveFails = 100;
    int framesRead = 0;
    int consecutiveFails = 0;
    bool depthWarningShown = false;

    while (framesRead < framesToRead)
    {
        cv::Mat frame;
        if (!source.readFrame(frame) || frame.empty())
        {
            // macOS potrafi opoznic pierwsze klatki - retry jak w pipeline
            if (++consecutiveFails > maxConsecutiveFails)
            {
                std::cerr << "[FAIL] readFrame keeps failing ("
                          << maxConsecutiveFails << " consecutive)" << std::endl;
                source.close();
                return 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        consecutiveFails = 0;
        ++framesRead;

        cv::Mat depth;
        if (!source.readDepth(depth))
        {
            if (!depthWarningShown)
            {
                std::cerr << "[WARN] depth not available yet" << std::endl;
                depthWarningShown = true;
            }
        }

        if (framesRead % 30 == 0)
        {
            std::cout << "frame " << framesRead << ": color " << frame.cols << "x" << frame.rows;
            if (!depth.empty())
                std::cout << ", depth " << depth.cols << "x" << depth.rows;
            std::cout << std::endl;
        }
    }

    source.close();
    std::cout << "[PASS] Read " << framesToRead << " frames" << std::endl;
    return 0;
}
