#include "app/VideoReceiveWorker.hpp"

#include <algorithm>
#include <utility>

namespace gcs::app {

VideoReceiveWorker::~VideoReceiveWorker()
{
    stop();
}

bool VideoReceiveWorker::start(std::uint16_t port, int timeout_ms)
{
    if (!receiver_.open(port)) {
        last_error_ = receiver_.lastError();
        return false;
    }

    running_ = true;
    worker_ = std::thread([this, timeout_ms]() {
        const int poll_timeout_ms = std::clamp(timeout_ms, 1, 50);
        while (running_) {
            auto frame = receiver_.receiveFrame(poll_timeout_ms);
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_ = receiver_.stats();
            }
            if (frame) {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                if (latest_frame_) {
                    ++overwritten_frames_;
                }
                latest_frame_ = std::move(frame);
                continue;
            }

            if (receiver_.lastError() != "timeout") {
                std::lock_guard<std::mutex> lock(error_mutex_);
                last_error_ = receiver_.lastError();
            }
        }
    });
    return true;
}

void VideoReceiveWorker::stop()
{
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    receiver_.close();
}

std::optional<video::JpegFrame> VideoReceiveWorker::takeLatestFrame()
{
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (!latest_frame_) {
        return std::nullopt;
    }
    auto output = std::move(latest_frame_);
    latest_frame_.reset();
    return output;
}

video::UdpMjpegReceiverStats VideoReceiveWorker::stats() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

std::uint64_t VideoReceiveWorker::overwrittenFrames() const
{
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return overwritten_frames_;
}

std::string VideoReceiveWorker::takeLastError()
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    std::string output = std::move(last_error_);
    last_error_.clear();
    return output;
}

} // namespace gcs::app
