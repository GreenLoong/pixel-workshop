#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <opencv2/core.hpp>

namespace ImageProcessor
{
// 输入：8 位、三通道 RGB 图片。
// 输出：同尺寸的8位、单通道灰度图片。
cv::Mat toGrayscale(const cv::Mat &rgb);
cv::Mat resizeByPercent(const cv::Mat &soure, int percent);
cv::Mat resizeToSize(const cv::Mat &source, cv::Size target);
}

#endif // IMAGEPROCESSOR_H