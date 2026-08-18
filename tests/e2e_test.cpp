#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cstdint>
#include <cstring>
#include <opencv2/opencv.hpp>
#include "H264Encoder.h"
#include "FileFrameSource.h"
#include "DjiPayloadSender.h"
#include "VideoStreamPipeline.h"

static bool fileSize(const std::string &path, long &size)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr)
        return false;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    return true;
}

static int countAud(const std::string &path)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr)
        return -1;
    std::vector<uint8_t> buf;
    uint8_t chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
        buf.insert(buf.end(), chunk, chunk + n);
    fclose(f);

    const uint8_t aud[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x10};
    int count = 0;
    for (size_t i = 0; i + 6 <= buf.size(); ++i)
    {
        if (memcmp(&buf[i], aud, 6) == 0)
            count++;
    }
    return count;
}

static bool ffprobeReport(const std::string &path)
{
    std::string cmd = "ffprobe -v error -show_entries stream=codec_name,width,height,r_frame_rate,nb_read_frames"
                      " -of default=noprint_wrappers=1 \"" + path + "\" 2>&1";
    int rc = system(cmd.c_str());
    return rc == 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <video_path>" << std::endl;
        return 2;
    }
    const std::string videoPath = argv[1];
    const std::string outPath = "stream_out.h264";

    // usun stary plik zanim zaczniemy
    remove(outPath.c_str());

    int passCount = 0;
    int failCount = 0;

    // ===== 1. Pipeline: wlacz, poczekaj, wylacz =====
    {
        auto source = std::unique_ptr<IFrameSource>(new FileFrameSource(videoPath));
        if (!source->open())
        {
            std::cerr << "[FAIL] open source: " << videoPath << std::endl;
            return 1;
        }
        std::cout << "Opened: " << source->getWidth() << "x" << source->getHeight()
                  << " @ " << source->getFps() << " fps" << std::endl;

        auto encoder = std::unique_ptr<H264Encoder>(
            new H264Encoder(source->getWidth(), source->getHeight(), source->getFps(), 4000000));
        if (!encoder->isReady())
        {
            std::cerr << "[FAIL] encoder not ready" << std::endl;
            return 1;
        }

        auto sender = std::unique_ptr<DjiPayloadSender>(new DjiPayloadSender());

        VideoStreamPipeline pipeline(std::move(source), std::move(encoder), std::move(sender));
        if (!pipeline.start())
        {
            std::cerr << "[FAIL] pipeline start" << std::endl;
            return 1;
        }

        std::cout << "Pipeline running 3s..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        pipeline.stop();

        std::cout << "[PASS] pipeline start/stop" << std::endl;
        passCount++;
    } // pipeline zniszczony tutaj => sender fclose + flush do pliku

    // ===== 2. Plik istnieje i nie jest pusty =====
    long size = 0;
    if (fileSize(outPath, size) && size > 0)
    {
        std::cout << "[PASS] stream_out.h264 exists, size=" << size << " B" << std::endl;
        passCount++;
    }
    else
    {
        std::cout << "[FAIL] stream_out.h264 missing/empty" << std::endl;
        failCount++;
    }

    // ===== 3. ffprobe: H.264, wymiary, fps =====
    std::cout << "--- ffprobe ---" << std::endl;
    if (ffprobeReport(outPath))
    {
        std::cout << "[PASS] ffprobe read stream" << std::endl;
        passCount++;
    }
    else
    {
        std::cout << "[FAIL] ffprobe could not read stream" << std::endl;
        failCount++;
    }

    // ===== 4. AUD markers =====
    int audCount = countAud(outPath);
    if (audCount > 0)
    {
        std::cout << "[PASS] AUD markers found: " << audCount << " (framing OK)" << std::endl;
        passCount++;
    }
    else
    {
        std::cout << "[FAIL] no AUD markers" << std::endl;
        failCount++;
    }

    std::cout << "\n=== KROK 7 E2E: " << passCount << " passed, " << failCount << " failed ===" << std::endl;
    return failCount == 0 ? 0 : 1;
}
