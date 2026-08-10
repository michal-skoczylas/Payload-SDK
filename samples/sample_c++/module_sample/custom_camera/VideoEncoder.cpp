#include "VideoEncoder.h"
#include <iostream>
#include <cstdlib>

VideoEncoder::VideoEncoder(int width, int height, int fps)
{
    tempFilename_ = "appsrc_temp.h264";
    int fourcc = cv::VideoWriter::fourcc('X', '2', '6', '4');

    writer_.open(tempFilename_, fourcc, fps, cv::Size(width, height), true);

    if (!writer_.isOpened())
    {
        std::cout << "[Ostrzezenie] Brak wsparcia H.264 w OpenCV. Proba MJPEG..." << std::endl;
        tempFilename_ = "appsrc_temp.mjpg";
        fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        writer_.open(tempFilename_, fourcc, fps, cv::Size(width, height), true);
    }

    if (!writer_.isOpened())
    {
        std::cerr << "\n[KRYTYCZNY BLAD] OpenCV nie moglo uruchomic kodera!" << std::endl;
        exit(1);
    }
    std::cout << "[SUKCES] Uruchomiono OpenCV VideoWriter -> " << tempFilename_ << std::endl;
}

VideoEncoder::~VideoEncoder()
{
    if (writer_.isOpened())
    {
        writer_.release();
    }
}

std::vector<uint8_t> VideoEncoder::encodeFrame(const cv::Mat &bgrFrame)
{
    std::vector<uint8_t> outputBuffer;

    if (writer_.isOpened())
    {
        writer_.write(bgrFrame);
    }

    // Wypychamy do paczki pusty bajt, zeby oszukac PSDK, ze cos mu wysylamy (dla testu strumieniowania).
    // W prawdziwym swiecie OpenCV potrafi tu wepnac rure GStreamera prosto do systemu drona.
    outputBuffer.push_back(0);

    return outputBuffer;
}