#ifndef IMAGECONVERSION_H
#define IMAGECONVERSION_H

#include <QImage>
#include <opencv2/core.hpp>

namespace ImageConversion {
// 借用 RGB888 / RGBA8888 数据；调用方必须让 QImage 活到处理结束。
cv::Mat rgbView(const QImage &image);
// 复制结果，QImage 的生命周期与 cv::Mat 独立。
QImage copyImage(const cv::Mat &image);
}
#endif
