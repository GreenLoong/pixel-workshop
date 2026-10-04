#include "imageprocessing.h"

#include <stdexcept>

QImage ImageProcessing::processImage(const QImage &original, const ImageProcessor::Options &options)
{
    if (original.isNull())
        throw std::runtime_error("The original image is empty");
    const bool alpha = original.hasAlphaChannel();
    const QImage rgb = original.convertToFormat(alpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
    if (rgb.isNull())
        throw std::runtime_error("Unable to convert the original image");
    const cv::Mat source(rgb.height(), rgb.width(), alpha ? CV_8UC4 : CV_8UC3,
                         const_cast<uchar *>(rgb.constBits()),
                         static_cast<size_t>(rgb.bytesPerLine()));
    const cv::Mat result = ImageProcessor::process(source, options);
    const QImage::Format format = result.channels() == 1
        ? QImage::Format_Grayscale8 : result.channels()==4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    // 结果复制为 Qt 自己管理的内存，避免 cv::Mat 析构后留下悬空指针。
    const QImage output = QImage(result.data, result.cols, result.rows,
                                static_cast<qsizetype>(result.step), format).copy();
    if (output.isNull())
        throw std::runtime_error("Unable to create the output image");
    return output;
}
