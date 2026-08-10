#include "FileFrameSource.h"

FileFrameSource::FileFrameSource(std::string path)
{
    this->path_ = path;
}

bool FileFrameSource::open()
{
    cap_.open(path_);
    if (cap_.isOpened())
    {
        this->width_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
        this->height_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
        this->fps_ = static_cast<int>(cap_.get(cv::CAP_PROP_FPS));
        if (fps_ <= 0)
        {
            fps_ = 30;
        }
        this->isOpen_ = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool FileFrameSource::readFrame(cv::Mat &frame)
{
    cap_ >> frame;
    // jesli plik sie skonczyl to przewijamy go na start
    if (frame.empty())
    {
        cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
        cap_ >> frame;
    }
    if (frame.empty())
    {
        return false;
    }

    return true;
}

void FileFrameSource::close()
{
    cap_.release();
    isOpen_ = false;
}

int FileFrameSource::getFps() const
{
    return fps_;
}

int FileFrameSource::getHeight() const
{
    return height_;
}

int FileFrameSource::getWidth() const
{
    return width_;
}