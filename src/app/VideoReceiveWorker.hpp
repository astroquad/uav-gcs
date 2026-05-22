#pragma once

#include "video/UdpMjpegReceiver.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace gcs::app {

class VideoReceiveWorker {
public:
    VideoReceiveWorker() = default;
    ~VideoReceiveWorker();

    VideoReceiveWorker(const VideoReceiveWorker&) = delete;
    VideoReceiveWorker& operator=(const VideoReceiveWorker&) = delete;

    bool start(std::uint16_t port, int timeout_ms);
    void stop();

    std::optional<video::JpegFrame> takeLatestFrame();
    video::UdpMjpegReceiverStats stats() const;
    std::uint64_t overwrittenFrames() const;
    std::string takeLastError();

private:
    video::UdpMjpegReceiver receiver_;
    std::atomic<bool> running_ {false};
    std::thread worker_;
    mutable std::mutex frame_mutex_;
    std::optional<video::JpegFrame> latest_frame_;
    std::uint64_t overwritten_frames_ = 0;
    mutable std::mutex stats_mutex_;
    video::UdpMjpegReceiverStats stats_;
    mutable std::mutex error_mutex_;
    std::string last_error_;
};

} // namespace gcs::app
