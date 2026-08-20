#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>

#include "RealsenseFrameSource.h"
#include "H264Encoder.h"

// Live TCP stream: RealSense -> H264Encoder -> TCP socket -> Mac (ffplay)
// RPi jest SERVEREM (listen), Mac sie laczy i dekoduje.
// Uzycie: stream_tcp_test [port] [width] [height] [fps] [bitrate]
//   domyslnie: 5555 1280 720 30 4000000

static const uint8_t kAud[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x10};

int main(int argc, char **argv)
{
    const int port = argc > 1 ? atoi(argv[1]) : 5555;
    const int width = argc > 2 ? atoi(argv[2]) : 1280;
    const int height = argc > 3 ? atoi(argv[3]) : 720;
    const int fps = argc > 4 ? atoi(argv[4]) : 30;
    const int bitrate = argc > 5 ? atoi(argv[5]) : 4000000;
    try
    {
        auto source = std::unique_ptr<IFrameSource>(new RealsenseFrameSource(width, height, fps));
        if (!source->open())
        {
            std::cerr << "camera open failed" << std::endl;
            return 1;
        }
        H264Encoder encoder(source->getWidth(), source->getHeight(), source->getFps(), bitrate);
        if (!encoder.isReady())
        {
            std::cerr << "encoder failed" << std::endl;
            return 1;
        }
        std::cout << "opened " << source->getWidth() << "x" << source->getHeight()
                  << "@" << source->getFps() << " enc=" << encoder.getEncoderName() << std::endl;

        int srv = socket(AF_INET, SOCK_STREAM, 0);
        int yes = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = INADDR_ANY;
        a.sin_port = htons(port);
        if (bind(srv, (sockaddr *)&a, sizeof(a)) < 0)
        {
            std::cerr << "bind failed" << std::endl;
            return 1;
        }
        listen(srv, 5);
        std::cout << "listening on :" << port << " ..." << std::endl;

        using clock = std::chrono::steady_clock;

        while (true)
        {
            int cli = accept(srv, nullptr, nullptr);
            if (cli < 0)
            {
                perror("accept");
                break;
            }
            std::cout << "client connected, streaming..." << std::endl;

            auto next = clock::now();
            const double period = 1.0 / source->getFps();
            size_t frames = 0, bytes = 0;
            auto logT = clock::now();

            while (true)
            {
                cv::Mat frame;
                if (!source->readFrame(frame))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                std::vector<uint8_t> nals = encoder.encodedFrame(frame);
                if (nals.empty())
                    continue;

                std::vector<uint8_t> unit(nals.size() + 6);
                memcpy(unit.data(), nals.data(), nals.size());
                memcpy(unit.data() + nals.size(), kAud, 6);

                ssize_t sent = send(cli, unit.data(), unit.size(), MSG_NOSIGNAL);
                if (sent < 0)
                {
                    std::cerr << "client disconnected" << std::endl;
                    break;
                }
                frames++;
                bytes += unit.size();

                auto now = clock::now();
                if (std::chrono::duration<double>(now - logT).count() >= 5.0)
                {
                    double dt = std::chrono::duration<double>(now - logT).count();
                    std::cout << (frames / dt) << " fps, " << (bytes * 8) / (dt * 1e6) << " Mbps" << std::endl;
                    frames = 0;
                    bytes = 0;
                    logT = now;
                }

                next += std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(period));
                if (next > clock::now())
                    std::this_thread::sleep_until(next);
                else
                    next = clock::now();
            }

            close(cli);
            std::cout << "waiting for next client..." << std::endl;
        }
        close(srv);
    }
    catch (const std::exception &e)
    {
        std::cerr << "err: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}