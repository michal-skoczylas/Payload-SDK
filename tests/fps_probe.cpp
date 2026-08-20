#include <iostream>
#include <thread>
#include <chrono>
#include <librealsense2/rs.hpp>

// Probe: ile fps koloru dostarcza kamera? Variant: z depth i BEZ depth.
// Uzycie: fps_probe <color_w> <color_h> <color_fps> [with_depth]
int main(int argc, char **argv)
{
    int w = argc > 1 ? atoi(argv[1]) : 640;
    int h = argc > 2 ? atoi(argv[2]) : 480;
    int f = argc > 3 ? atoi(argv[3]) : 60;
    bool withDepth = argc > 4 ? atoi(argv[4]) != 0 : true;

    rs2::pipeline pipe;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_COLOR, w, h, RS2_FORMAT_BGR8, f);
    if (withDepth)
        cfg.enable_stream(RS2_STREAM_DEPTH, 1280, 720, RS2_FORMAT_Z16, 30);
    pipe.start(cfg);

    // rozgrzewka 2 s
    auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 2.0)
    {
        try { pipe.wait_for_frames(100); } catch (const rs2::error &) {}
    }

    int n = 0;
    t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 10.0)
    {
        auto loopStart = std::chrono::steady_clock::now();
        while (std::chrono::duration<double>(std::chrono::steady_clock::now() - loopStart).count() < 1.0)
        {
            try
            {
                rs2::frameset fs = pipe.wait_for_frames(100);
                if (fs.get_color_frame())
                    n++;
            }
            catch (const rs2::error &) {}
        }
        double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << "color fps so far=" << (n / dt) << std::endl;
    }
    double final = n / std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << (withDepth ? "Z DEPTH  " : "BEZ DEPTH") << " " << w << "x" << h << "@" << f
              << ": " << n << " color klatek / " << final << " fps" << std::endl;
    pipe.stop();
    return 0;
}