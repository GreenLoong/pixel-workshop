#include "infrastructure/imageconversion.h"

#include <stdexcept>

cv::Mat ImageConversion::rgbView(const QImage &image)
{
    if(image.isNull() || (image.format()!=QImage::Format_RGB888 && image.format()!=QImage::Format_RGBA8888))
        throw std::runtime_error("Expected an RGB888 or RGBA8888 image");
    return cv::Mat(image.height(),image.width(),image.format()==QImage::Format_RGBA8888?CV_8UC4:CV_8UC3,
                   const_cast<uchar *>(image.constBits()),static_cast<size_t>(image.bytesPerLine()));
}

QImage ImageConversion::copyImage(const cv::Mat &image)
{
    if(image.empty() || image.depth()!=CV_8U || (image.channels()!=1 && image.channels()!=3 && image.channels()!=4))
        throw std::runtime_error("Expected an 8-bit grayscale, RGB or RGBA result");
    const auto format=image.channels()==1?QImage::Format_Grayscale8
        :image.channels()==4?QImage::Format_RGBA8888:QImage::Format_RGB888;
    QImage output=QImage(image.data,image.cols,image.rows,static_cast<qsizetype>(image.step),format).copy();
    if(output.isNull())throw std::runtime_error("Unable to create the output image");
    return output;
}
