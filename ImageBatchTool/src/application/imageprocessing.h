#ifndef IMAGEPROCESSING_H
#define IMAGEPROCESSING_H

#include "domain/imageprocessor.h"
#include <QImage>

// Qt 与 OpenCV 的数据转换集中在这里，不访问窗口或控件。
namespace ImageProcessing
{
QImage processImage(const QImage &original, const ImageProcessor::Options &options);
}
#endif
