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
    double exposure = 0.0; // -2～2 EV
    int saturation = 0, temperature = 0, tint = 0;
    int highlights = 0, shadows = 0, clarity = 0, vignette = 0; // -100～100
    bool hasColorAdjustments() const {
        return exposure!=0 || saturation!=0 || temperature!=0 || tint!=0
            || highlights!=0 || shadows!=0 || clarity!=0 || vignette!=0;
    }
    void clearTone() {
        brightness=0; contrast=1; exposure=0;
        saturation=temperature=tint=highlights=shadows=clarity=vignette=0;
    }
    double rotation = 0.0; // 顺时针角度，-180～180；展开画布保留完整图片。
    bool flipHorizontal = false;
    bool flipVertical = false;
    cv::Rect2d crop; // 旋转／翻转之后的归一化区域；空矩形表示不裁剪。
    bool isIdentity(cv::Size originalSize) const
    {
        return !hasColorAdjustments() && rotation == 0 && !flipHorizontal && !flipVertical && crop == cv::Rect2d()
               && !grayscale && brightness == 0 && contrast == 1.0
               && (targetSize == cv::Size() || targetSize == originalSize);
    }
};

// 固定顺序：灰度化、调整像素尺寸、亮度和对比度。
// 输入为 8 位 RGB；结果拥有独立的数据，不修改输入。
cv::Mat process(const cv::Mat &rgb, const Options &options);
cv::Mat transformGeometry(const cv::Mat &source, const Options &options);
cv::Mat adjustColor(const cv::Mat &rgb, const Options &options);
cv::Mat adjustTone(const cv::Mat &source, int brightness, double contrast);
// 输入：8 位、三通道 RGB 图片。
// 输出：同尺寸的8位、单通道灰度图片。
cv::Mat toGrayscale(const cv::Mat &rgb);
cv::Mat resizeByPercent(const cv::Mat &source, int percent);
cv::Mat resizeToSize(const cv::Mat &source, cv::Size target);
}

#endif // IMAGEPROCESSOR_H
