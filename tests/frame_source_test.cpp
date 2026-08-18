#include <iostream>
#include <string>

#include "FileFrameSource.h"
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_path>" << std::endl;
        return 2;
    }
    const std::string videoPath = argv[1];

    FileFrameSource source(videoPath);

    if (!source.open())
    {
        std::cerr << "[FAIL] Could not open: " << videoPath << std::endl;
        return 1;
    }
    std::cout << "Opened: " << source.getWidth() << "x" << source.getHeight()
              << " @ " << source.getFps() << " fps" << std::endl;
    const int framesToRead = 400;
    for (int i = 0; i < framesToRead; ++i)
    {
        cv::Mat frame;
        if (!source.readFrame(frame) || frame.empty())
        {
            std::cerr << "[FAIL] readFrame failed at frame " << i << std::endl;
            return 1;
        }
        if (i % 50 == 0)
        {
            std::cout << "frame " << i << ": " << frame.cols << "x" << frame.rows << std::endl;
        }
    }
    source.close();
    std::cout << "[PASS] Read " << framesToRead << " frames (loop-back works)" << std::endl;
    return 0;
}