#include "imageprocessor.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

// 灰度化
cv::Mat ImageProcessor::toGrayscale(const cv::Mat &rgb)
{
    cv::Mat gray;
    cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);

    return gray;
}

// 缩放
cv::Mat ImageProcessor::resizeByPercent(const cv::Mat &source, int percent)
{
    // 参数匹配，不符合抛出 OpenCV 异常
    if (source.empty() || percent < 1 || percent > 200)
        CV_Error(cv::Error::StsBadArg, "图片不能为空，百分比必须在1到200之间");

    const double scale = percent / 100.0;

    const int width = std::max(1, static_cast<int>(std::round(source.cols * scale)));

    const int height = std::max(1, static_cast<int>(std::round(source.rows * scale)));

    // 缩小时使用面积插值，放大时使用线性插值
    const int interpolation = percent < 100 ? cv::INTER_AREA : cv::INTER_LINEAR;

    cv::Mat result;

    cv::resize(source, result, cv::Size(width, height), 0, 0, interpolation);

    return result;
}

cv::Mat ImageProcessor::resizeToSize(const cv::Mat &source, cv::Size target)
{
    const long long pixels = static_cast<long long>(target.width) * target.height;

    if (source.empty() || target.width <= 0 || target.height <= 0 || pixels > 40000000)
    {
        CV_Error( cv::Error::StsBadArg, "无效的图片或目标尺寸");
    }

    const bool shrinking = target.width <= source.cols && target.height <= source.rows;

    cv::Mat result;

    cv::resize(source, result, target, 0, 0, shrinking ? cv::INTER_AREA : cv::INTER_LINEAR);

    return result;
}