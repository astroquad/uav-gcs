#include "ui/VideoWindow.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cstdint>
#include <utility>

namespace gcs::ui {
namespace {

std::int64_t unixTimestampMs()
{
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return ms.count();
}

} // namespace

VideoWindow::VideoWindow(std::string title)
    : title_(std::move(title))
{
    cv::namedWindow(title_, cv::WINDOW_AUTOSIZE);
}

VideoWindow::~VideoWindow()
{
    cv::destroyWindow(title_);
}

bool VideoWindow::showFrame(const video::JpegFrame& frame)
{
    const cv::Mat encoded(1, static_cast<int>(frame.data.size()), CV_8UC1, const_cast<std::uint8_t*>(frame.data.data()));
    cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
    if (image.empty()) {
        return false;
    }

    const auto latency_ms = unixTimestampMs() - static_cast<std::int64_t>(frame.timestamp_ms);
    cv::putText(
        image,
        "frame " + std::to_string(frame.frame_id) + " latency " + std::to_string(latency_ms) + " ms",
        cv::Point(12, 24),
        cv::FONT_HERSHEY_SIMPLEX,
        0.55,
        cv::Scalar(0, 255, 0),
        1,
        cv::LINE_AA);
    cv::imshow(title_, image);
    return true;
}

void VideoWindow::showStatus(const std::string& status)
{
    cv::Mat image(240, 480, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::putText(
        image,
        status,
        cv::Point(16, 120),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(0, 255, 255),
        1,
        cv::LINE_AA);
    cv::imshow(title_, image);
}

bool VideoWindow::shouldClose(int wait_ms)
{
    const int key = cv::waitKey(wait_ms);
    return key == 27 || key == 'q' || key == 'Q';
}

} // namespace gcs::ui
