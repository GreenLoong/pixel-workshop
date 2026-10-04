#include "domain/imageprocessor.h"

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
    if (rgb.empty() || (rgb.type() != CV_8UC3 && rgb.type() != CV_8UC4))
        CV_Error(cv::Error::StsBadArg, "Expected an 8-bit RGB or RGBA image");
    cv::Mat geometry = transformGeometry(rgb, options);
    const cv::Size target = options.targetSize == cv::Size() ? geometry.size() : options.targetSize;
    if (target.width <= 0 || target.height <= 0
        || static_cast<long long>(target.width) * target.height > 40000000)
        CV_Error(cv::Error::StsBadArg, "Invalid output size (maximum 40 million pixels)");
    if (options.brightness < -100 || options.brightness > 100
        || !std::isfinite(options.contrast) || options.contrast < 0.5 || options.contrast > 2.0)
        CV_Error(cv::Error::StsBadArg, "Invalid tone parameters");
    cv::Mat result = geometry;
    if (target != geometry.size())
        result = resizeToSize(result, target);
    result = processBackground(result, options);
    cv::Mat alpha;
    if (result.channels() == 4) {
        cv::extractChannel(result, alpha, 3);
        cv::cvtColor(result, result, cv::COLOR_RGBA2RGB);
    }
    result = adjustColor(result, options);
    if(options.grayscale) result=toGrayscale(result);
    if (options.brightness != 0 || options.contrast != 1.0)
        result = adjustTone(result, options.brightness, options.contrast);
    if (!alpha.empty()) {
        if (result.channels() == 1) cv::cvtColor(result,result,cv::COLOR_GRAY2RGB);
        cv::cvtColor(result,result,cv::COLOR_RGB2RGBA);
        cv::insertChannel(alpha,result,3);
    }
    return result.data == rgb.data ? result.clone() : result;
}

cv::Mat ImageProcessor::adjustColor(const cv::Mat &rgb, const Options &o)
{
    if(!std::isfinite(o.exposure) || o.exposure < -2 || o.exposure > 2)
        CV_Error(cv::Error::StsBadArg,"Invalid exposure");
    for(int value : {o.saturation,o.temperature,o.tint,o.highlights,o.shadows,o.clarity,o.vignette})
        if(value < -100 || value > 100) CV_Error(cv::Error::StsBadArg,"Invalid color parameter");
    if(!o.hasColorAdjustments())return rgb;
    cv::Mat f;
    rgb.convertTo(f,CV_32FC3,1.0/255);
    f *= std::pow(2.0,o.exposure);
    if(o.clarity!=0) {
        cv::Mat blurred;
        cv::GaussianBlur(f,blurred,cv::Size(),2);
        f += (f-blurred)*(o.clarity/100.0);
    }
    for(int y=0;y<f.rows;++y)for(int x=0;x<f.cols;++x) {
        auto &p=f.at<cv::Vec3f>(y,x);
        const float l=std::clamp(0.299f*p[0]+0.587f*p[1]+0.114f*p[2],0.f,1.f);
        const float tone=o.shadows/200.f*(1-l)*(1-l)+o.highlights/200.f*l*l;
        p += cv::Vec3f(tone+o.temperature/1000.f+o.tint/1250.f,
                      tone-o.tint/1250.f,tone-o.temperature/1000.f+o.tint/1250.f);
        if(o.vignette!=0) {
            const double dx=(x-(f.cols-1)/2.0)/std::max(1.0,f.cols/2.0);
            const double dy=(y-(f.rows-1)/2.0)/std::max(1.0,f.rows/2.0);
            p *= static_cast<float>(std::max(0.0,1-o.vignette/200.0*(dx*dx+dy*dy)));
        }
    }
    cv::max(f,0,f); cv::min(f,1,f);
    if(o.saturation!=0) {
        cv::Mat hsv; cv::cvtColor(f,hsv,cv::COLOR_RGB2HSV);
        for(int y=0;y<hsv.rows;++y)for(int x=0;x<hsv.cols;++x)
            hsv.at<cv::Vec3f>(y,x)[1]=std::clamp(hsv.at<cv::Vec3f>(y,x)[1]*(1+o.saturation/100.f),0.f,1.f);
        cv::cvtColor(hsv,f,cv::COLOR_HSV2RGB);
    }
    cv::Mat result; f.convertTo(result,CV_8UC3,255);
    return result;
}

cv::Mat ImageProcessor::transformGeometry(const cv::Mat &source, const Options &options)
{
    if (source.empty() || !std::isfinite(options.rotation)
        || options.rotation < -180 || options.rotation > 180)
        CV_Error(cv::Error::StsBadArg, "Invalid rotation");
    cv::Mat result = source.clone();
    // 输出单独分配，避免翻转写入原图。
    if (options.flipHorizontal || options.flipVertical) {
        cv::flip(source, result, options.flipHorizontal && options.flipVertical ? -1
                  : options.flipHorizontal ? 1 : 0);
    }
    const double angle = options.rotation;
    if (angle == 90 || angle == -90 || std::abs(angle) == 180) {
        cv::rotate(result, result, std::abs(angle) == 180 ? cv::ROTATE_180
            : angle == 90 ? cv::ROTATE_90_CLOCKWISE : cv::ROTATE_90_COUNTERCLOCKWISE);
    } else if (angle != 0) {
        auto matrix = cv::getRotationMatrix2D(
            cv::Point2f((result.cols - 1) / 2.0f, (result.rows - 1) / 2.0f), -angle, 1);
        const double radians = angle * CV_PI / 180.0;
        const int width = static_cast<int>(std::ceil(std::abs(result.cols * std::cos(radians))
                                                + std::abs(result.rows * std::sin(radians))));
        const int height = static_cast<int>(std::ceil(std::abs(result.rows * std::cos(radians))
                                                 + std::abs(result.cols * std::sin(radians))));
        if (static_cast<long long>(width) * height > 40000000)
            CV_Error(cv::Error::StsBadArg, "Rotated canvas exceeds 40 million pixels");
        matrix.at<double>(0,2) += (width - result.cols) / 2.0;
        matrix.at<double>(1,2) += (height - result.rows) / 2.0;
        cv::Mat rotated;
        cv::warpAffine(result, rotated, matrix, cv::Size(width,height),
            cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255,255,255,0));
        result = rotated;
    }
    const auto &c = options.crop;
    if (c != cv::Rect2d()) {
        if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.width)
            || !std::isfinite(c.height) || c.x < 0 || c.y < 0
            || c.width <= 0 || c.height <= 0 || c.x + c.width > 1.000001
            || c.y + c.height > 1.000001)
            CV_Error(cv::Error::StsBadArg, "Invalid normalized crop");
        const int x = std::clamp(static_cast<int>(std::floor(c.x * result.cols)), 0, result.cols - 1);
        const int y = std::clamp(static_cast<int>(std::floor(c.y * result.rows)), 0, result.rows - 1);
        const int right = std::clamp(static_cast<int>(std::ceil((c.x+c.width) * result.cols)), x+1, result.cols);
        const int bottom = std::clamp(static_cast<int>(std::ceil((c.y+c.height) * result.rows)), y+1, result.rows);
        result = result(cv::Rect(x,y,right-x,bottom-y)).clone();
    }
    return result.data == source.data ? result.clone() : result;
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
