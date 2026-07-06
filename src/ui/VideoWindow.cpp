#include "ui/VideoWindow.hpp"

#include "overlay/OverlayMapping.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace gcs::ui {
namespace {

cv::Scalar toScalar(const overlay::Color& color)
{
    return cv::Scalar(color.b, color.g, color.r);
}

cv::Point toPoint(const overlay::Point2f& point)
{
    return cv::Point(
        static_cast<int>(std::lround(point.x)),
        static_cast<int>(std::lround(point.y)));
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
    return showFrame(frame, {});
}

bool VideoWindow::showFrame(
    const video::JpegFrame& frame,
    const std::vector<overlay::OverlayPrimitive>& overlays)
{
    return showFrame(frame, overlays, 0, 0);
}

bool VideoWindow::showFrame(
    const video::JpegFrame& frame,
    const std::vector<overlay::OverlayPrimitive>& overlays,
    int overlay_space_width,
    int overlay_space_height)
{
    const cv::Mat encoded(1, static_cast<int>(frame.data.size()), CV_8UC1, const_cast<std::uint8_t*>(frame.data.data()));
    cv::Mat image = cv::imdecode(encoded, cv::IMREAD_COLOR);
    if (image.empty()) {
        return false;
    }

    // Overlay coordinates live in the onboard camera pixel space, which is
    // larger than the decoded image when the sender downscales debug video.
    const int space_width = overlay::resolveOverlaySpace(overlay_space_width, image.cols);
    const int space_height = overlay::resolveOverlaySpace(overlay_space_height, image.rows);
    const auto mapPoint = [&](const overlay::Point2f& point) {
        return toPoint(overlay::mapToDisplay(
            point, space_width, space_height, 0, 0, image.cols, image.rows));
    };

    cv::putText(
        image,
        "frame " + std::to_string(frame.frame_id),
        cv::Point(12, 24),
        cv::FONT_HERSHEY_SIMPLEX,
        0.55,
        cv::Scalar(0, 255, 0),
        1,
        cv::LINE_AA);

    for (const auto& overlay : overlays) {
        switch (overlay.type) {
        case overlay::OverlayPrimitive::Type::Line:
            cv::line(
                image,
                mapPoint(overlay.line.start),
                mapPoint(overlay.line.end),
                toScalar(overlay.line.color),
                overlay.line.thickness,
                cv::LINE_AA);
            break;
        case overlay::OverlayPrimitive::Type::Circle:
            cv::circle(
                image,
                mapPoint(overlay.circle.center),
                std::max(1, static_cast<int>(std::lround(overlay::mapLength(
                    overlay.circle.radius, space_width, image.cols)))),
                toScalar(overlay.circle.color),
                overlay.circle.thickness,
                cv::LINE_AA);
            break;
        case overlay::OverlayPrimitive::Type::Text:
            cv::putText(
                image,
                overlay.text.text,
                mapPoint(overlay.text.origin),
                cv::FONT_HERSHEY_SIMPLEX,
                overlay.text.scale,
                toScalar(overlay.text.color),
                overlay.text.thickness,
                cv::LINE_AA);
            break;
        }
    }

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
