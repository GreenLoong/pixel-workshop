#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <opencv2/core.hpp>
#include <vector>
#include <memory>
#include <tuple>

namespace ImageProcessor
{
enum class BackgroundMode { None, Blur, Remove, Replace };
enum class SegmentationMethod { Human, Region };
struct BrushStroke {
    std::vector<cv::Point2d> points; // 相对于处理后图片的归一化坐标。
    double radius = 0.025;
    bool foreground = true;
    bool operator==(const BrushStroke &other) const {
        return points==other.points && radius==other.radius && foreground==other.foreground;
    }
};
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
    BackgroundMode background = BackgroundMode::None;
    SegmentationMethod segmentation = SegmentationMethod::Human;
    std::shared_ptr<const std::vector<uchar>> humanModel; // Qt 适配层提供只读模型字节。
    cv::Rect2d foregroundRect{0.1,0.05,0.8,0.9};
    std::vector<BrushStroke> strokes;
    int feather = 2; // 在分割预览尺度上的羽化像素。
    int backgroundBlur = 15;
    cv::Scalar backgroundColor{255,255,255}; // RGB
    cv::Mat backgroundImage; // 只读共享，修改时替换整份图片。
    bool operator==(const Options &o) const {
        const auto fields=[](const Options &v) {
            return std::tie(v.grayscale,v.targetSize,v.brightness,v.contrast,v.exposure,
                v.saturation,v.temperature,v.tint,v.highlights,v.shadows,v.clarity,v.vignette,
                v.rotation,v.flipHorizontal,v.flipVertical,v.crop,v.background,v.segmentation,
                v.humanModel,v.foregroundRect,v.strokes,v.feather,v.backgroundBlur,v.backgroundColor);
        };
        return fields(*this)==fields(o) && backgroundImage.data==o.backgroundImage.data;
    }
    bool isIdentity(cv::Size originalSize) const
    {
        return background == BackgroundMode::None && !hasColorAdjustments() && rotation == 0 && !flipHorizontal && !flipVertical && crop == cv::Rect2d()
               && !grayscale && brightness == 0 && contrast == 1.0
               && (targetSize == cv::Size() || targetSize == originalSize);
    }
};

// 顺序：几何、尺寸、背景、颜色、灰度、亮度对比度。
// 输入为 8 位 RGB 或 RGBA；结果拥有独立的数据，不修改输入。
cv::Mat process(const cv::Mat &rgb, const Options &options);
cv::Mat transformGeometry(const cv::Mat &source, const Options &options);
cv::Mat adjustColor(const cv::Mat &rgb, const Options &options);
cv::Mat processBackground(const cv::Mat &source, const Options &options);
struct RegionSamples {
    int foreground=0, background=0;
    bool canSegment() const { return foreground>=5 && background>=5; }
};
// 与区域分割使用相同的缩略尺寸和画笔规则，供界面解释手工蒙版回退。
RegionSamples regionSamples(cv::Size imageSize,const Options &options);
cv::Mat adjustTone(const cv::Mat &source, int brightness, double contrast);
// 输入：8 位、三通道 RGB 图片。
// 输出：同尺寸的8位、单通道灰度图片。
cv::Mat toGrayscale(const cv::Mat &rgb);
cv::Mat resizeByPercent(const cv::Mat &source, int percent);
cv::Mat resizeToSize(const cv::Mat &source, cv::Size target);
}

#endif // IMAGEPROCESSOR_H
