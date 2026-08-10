#include <iostream>
#include "H264Encoder.h"
#include "FileFrameSource.h"
#include <string>

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

    // utworzenie kodera
    H264Encoder encoder(source.getWidth(), source.getHeight(), source.getFps(), 4000000);
    if (!encoder.isReady())
    {
        std::cerr << "Encoder failed to be ready" << std::endl;

        return 1;
    }
    // otwarcie pliku wyjściowego
    FILE *f = fopen("/tmp/out.h264", "wb");
    if (!f)
    {
        std::cerr << "Failed to open stream file" << std::endl;
        return 1;
    }
    // petla encodowania
    // TODO: to zmienic
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
        fwrite(nals.data(), 1, nals.size(), f);
        if (i % 10 == 0)
        {
            std::cout << "frame " << i << ": encoded " << nals.size() << " bytes" << std::endl;
        }
    }

    fclose(f);
    source.close();
    std::cout << "[PASS] Encoded " << frames << " frames to /tmp/out.h264" << std::endl;
    return 0;
}
