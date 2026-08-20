#ifdef REALSENSE_INSTALLED
#include "RealsenseFrameSource.h"
#include <iostream>

RealsenseFrameSource::RealsenseFrameSource(int width, int height, int fps)
{
    this->width_ = width;
    this->height_ = height;
    this->fps_ = fps;
    this->isOpen_ = false;
}

bool RealsenseFrameSource::open()
{
    try
    {
        // enable stream for color
        this->cfg_.enable_stream(RS2_STREAM_COLOR, width_, height_, RS2_FORMAT_BGR8, fps_);
        // enable stream for depth
        this->cfg_.enable_stream(RS2_STREAM_DEPTH, 1280, 720, RS2_FORMAT_Z16, 30);
        this->pipe_.start(this->cfg_);

        // zapamietaj zadane parametry zanim zostana nadpisane rzeczywistymi z kamery
        const int reqWidth = this->width_;
        const int reqHeight = this->height_;
        const int reqFps = this->fps_;

        auto profile = this->pipe_.get_active_profile().get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>();
        this->width_ = profile.width();
        this->height_ = profile.height();
        this->fps_ = profile.fps();
        if (this->width_ != reqWidth || this->height_ != reqHeight || this->fps_ != reqFps)
        {
            std::cerr << "RealSense: zadano " << reqWidth << "x" << reqHeight << "@" << reqFps
                      << "fps, kamera dal " << this->width_ << "x" << this->height_
                      << "@" << this->fps_ << "fps" << std::endl;
        }
        this->isOpen_ = true;
        return true;
    }
    catch (const rs2::error &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

bool RealsenseFrameSource::readFrame(cv::Mat &frame)
{
    try
    {
        rs2::frameset fs = this->pipe_.wait_for_frames(100);
        rs2::video_frame color = fs.get_color_frame();
        if (!color)
        {
            return false;
        }
        cv::Mat tmp(cv::Size(color.get_width(), color.get_height()), CV_8UC3, (void *)color.get_data(), color.get_stride_in_bytes());
        tmp.copyTo(frame);
        rs2::depth_frame depth = fs.get_depth_frame();
        if (depth)
        {
            cv::Mat tmpDepth(cv::Size(depth.get_width(), depth.get_height()), CV_16UC1, (void *)depth.get_data(), depth.get_stride_in_bytes());
            tmpDepth.copyTo(this->depth_);
        }
        return true;
    }
    catch (rs2::error &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

bool RealsenseFrameSource::readDepth(cv::Mat &depth)
{
    if (this->depth_.empty())
        return false;
    this->depth_.copyTo(depth);
    return true;
}
void RealsenseFrameSource::close()
{
    if (this->isOpen_)
    {
        this->pipe_.stop();
        this->isOpen_ = false;
    }
}

RealsenseFrameSource::~RealsenseFrameSource() { close(); }

int RealsenseFrameSource::getFps() const
{
    return this->fps_;
}

int RealsenseFrameSource::getHeight() const
{
    return this->height_;
}

int RealsenseFrameSource::getWidth() const
{
    return this->width_;
}

#endif // REALSENSE_INSTALLED