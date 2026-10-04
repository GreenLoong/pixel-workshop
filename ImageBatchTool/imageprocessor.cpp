#include "imageprocessor.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

cv::Mat ImageProcessor::adjustTone(const cv::Mat &source, int brightness, double contrast)
{
    if (source.empty() || source.depth() != CV_8U
        || (source.channels() != 1 && source.channels() != 3)
        || brightness < -100 || brightness > 100
        || !std::isfinite(contrast) || contrast < 0.5 || contrast > 2.0)
        CV_Error(cv::Error::StsBadArg, "Invalid image or tone parameters");
    cv::Mat result;
    // convertTo 对 8 位输出执行饱和转换，越界值限制在 0～255。
    source.convertTo(result, -1, contrast, brightness);
    return result;
}

cv::Mat ImageProcessor::process(const cv::Mat &rgb, const Options &options)
{
    if (rgb.empty() || rgb.type() != CV_8UC3)
        CV_Error(cv::Error::StsBadArg, "Expected an 8-bit RGB image");
    const cv::Size target = options.targetSize == cv::Size() ? rgb.size() : options.targetSize;
    if (target.width <= 0 || target.height <= 0
        || static_cast<long long>(target.width) * target.height > 40000000)
        CV_Error(cv::Error::StsBadArg, "Invalid output size (maximum 40 million pixels)");
    if (options.brightness < -100 || options.brightness > 100
        || !std::isfinite(options.contrast) || options.contrast < 0.5 || options.contrast > 2.0)
        CV_Error(cv::Error::StsBadArg, "Invalid tone parameters");
    cv::Mat result = options.grayscale ? toGrayscale(rgb) : rgb;
    if (target != rgb.size())
        result = resizeToSize(result, target);
    if (options.brightness != 0 || options.contrast != 1.0)
        result = adjustTone(result, options.brightness, options.contrast);
    return result.data == rgb.data ? result.clone() : result;
}

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

    return resizeToSize(source, cv::Size(width, height));
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
