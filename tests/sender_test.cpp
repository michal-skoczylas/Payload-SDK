#include <iostream>
#include <string>
#include <opencv2/opencv.hpp>
#include "H264Encoder.h"
#include "FileFrameSource.h"
#include "DjiPayloadSender.h"

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

    H264Encoder encoder(source.getWidth(), source.getHeight(), source.getFps(), 4000000);
    if (!encoder.isReady())
    {
        std::cerr << "Encoder failed to be ready" << std::endl;
        return 1;
    }

    DjiPayloadSender sender;

    const int frames = 60;
    for (int i = 0; i < frames; ++i)
    {
        cv::Mat frame;
        if (!source.readFrame(frame) || frame.empty())
        {
            std::cerr << "Empty frame " << std::endl;
            return 1;
        }
        std::vector<uint8_t> nals = encoder.encodedFrame(frame);
        if (!sender.send(nals))
        {
            std::cerr << "[FAIL] send failed at frame " << i << std::endl;
            return 1;
        }
        if (i % 10 == 0)
        {
            std::cout << "frame " << i << ": sent " << nals.size() << " bytes" << std::endl;
        }
    }

    source.close();
    std::cout << "[PASS] Encoded " << frames << " frames to stream_out.h264" << std::endl;
    return 0;
}
