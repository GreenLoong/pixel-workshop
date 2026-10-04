#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <opencv2/core.hpp>

namespace ImageProcessor
{
// 全部处理参数放在一个值对象中，不依赖窗口或控件。
struct Options
{
    bool grayscale = false;
    cv::Size targetSize; // 空尺寸表示保持原图像素尺寸。
    int brightness = 0;
    double contrast = 1.0;
    bool isIdentity(cv::Size originalSize) const
    {
        return !grayscale && brightness == 0 && contrast == 1.0
               && (targetSize == cv::Size() || targetSize == originalSize);
    }
};

// 固定顺序：灰度化、调整像素尺寸、亮度和对比度。
// 输入为 8 位 RGB；结果拥有独立的数据，不修改输入。
cv::Mat process(const cv::Mat &rgb, const Options &options);
cv::Mat adjustTone(const cv::Mat &source, int brightness, double contrast);
// 输入：8 位、三通道 RGB 图片。
// 输出：同尺寸的8位、单通道灰度图片。
cv::Mat toGrayscale(const cv::Mat &rgb);
cv::Mat resizeByPercent(const cv::Mat &source, int percent);
cv::Mat resizeToSize(const cv::Mat &source, cv::Size target);
}

#endif // IMAGEPROCESSOR_H
