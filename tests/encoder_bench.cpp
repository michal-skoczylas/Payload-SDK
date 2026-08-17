#include <iostream>
#include <string>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <thread>
#include <cstdio>
#include <vector>
#include <cstdint>

#include "IFrameSource.h"
#include "FileFrameSource.h"
#include "RealsenseFrameSource.h"
#include "H264Encoder.h"

// Encoder bench: mierzy fps / czas kodowania / bitrate dla wybranego backendu.
// Uzycie: encoder_bench <source> <w> <h> <fps> <bitrate> <backend> [frames] [outfile]
//   source:  "cam" albo "file:<sciezka>"
//   backend: "auto" | "v4l2" | "x264"
// Kompiluj z -DREALSENSE_INSTALLED (RealsenseFrameSource istnieje tylko wtedy).
int main(int argc, char **argv)
{
    if (argc < 7)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <cam|file:path> <w> <h> <fps> <bitrate> <auto|v4l2|x264> [frames] [outfile]"
                  << std::endl;
        return 2;
    }
    const std::string sourceArg = argv[1];
    const int w = atoi(argv[2]);
    const int h = atoi(argv[3]);
    const int fps = atoi(argv[4]);
    const int bitrate = atoi(argv[5]);
    const std::string backendArg = argv[6];
    const int framesTotal = argc > 7 ? atoi(argv[7]) : 300;
    const char *outPath = argc > 8 ? argv[8] : nullptr;

    EncoderBackend backend = ENCODER_AUTO;
    if (backendArg == "v4l2")
        backend = ENCODER_V4L2M2M;
    else if (backendArg == "x264")
        backend = ENCODER_LIBX264;

    std::unique_ptr<IFrameSource> source;
    if (sourceArg == "cam")
        source.reset(new RealsenseFrameSource(w, h, fps));
    else if (sourceArg.rfind("file:", 0) == 0)
        source.reset(new FileFrameSource(sourceArg.substr(5)));
    else
    {
        std::cerr << "zle zrodlo: " << sourceArg << std::endl;
        return 2;
    }

    if (!source->open())
    {
        std::cerr << "[FAIL] nie udalo sie otworzyc zrodla" << std::endl;
        return 1;
    }
    std::cout << "source: " << source->getWidth() << "x" << source->getHeight()
              << " @" << source->getFps() << " fps" << std::endl;

    H264Encoder encoder(w, h, fps, bitrate, backend);
    if (!encoder.isReady())
    {
        std::cerr << "[FAIL] enkoder nie gotowy" << std::endl;
        return 1;
    }
    std::cout << "backend: " << encoder.getEncoderName() << std::endl;

    FILE *out = nullptr;
    if (outPath)
    {
        out = fopen(outPath, "wb");
        if (!out)
        {
            std::cerr << "[FAIL] nie mozna otworzyc pliku wyjsciowego" << std::endl;
            return 1;
        }
    }

    static const uint8_t kAud[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x10};

    int framesDone = 0;
    long long bytesOut = 0;
    double encodeMsTotal = 0.0;
    auto t0 = std::chrono::steady_clock::now();

    while (framesDone < framesTotal)
    {
        cv::Mat frame;
        if (!source->readFrame(frame) || frame.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        auto e0 = std::chrono::steady_clock::now();
        std::vector<uint8_t> nals = encoder.encodedFrame(frame);
        auto e1 = std::chrono::steady_clock::now();
        encodeMsTotal += std::chrono::duration<double, std::milli>(e1 - e0).count();

        bytesOut += nals.size() + 6;
        ++framesDone;

        if (out)
        {
            fwrite(nals.data(), 1, nals.size(), out);
            fwrite(kAud, 1, 6, out);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double wallS = std::chrono::duration<double>(t1 - t0).count();

    if (out)
        fclose(out);

    std::cout << "frames: " << framesDone << std::endl;
    std::cout << "wall: " << wallS << " s" << std::endl;
    std::cout << "fps (wall): " << (framesDone / wallS) << std::endl;
    std::cout << "encode: " << (encodeMsTotal / framesDone) << " ms/klatke (tylko encodeFrame)" << std::endl;
    std::cout << "fps (encode only): " << (1000.0 * framesDone / encodeMsTotal) << std::endl;
    std::cout << "bitrate: " << (bytesOut * 8.0 / wallS / 1000.0) << " kb/s" << std::endl;

    return 0;
}
