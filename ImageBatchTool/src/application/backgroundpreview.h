#ifndef BACKGROUNDPREVIEW_H
#define BACKGROUNDPREVIEW_H

#include "domain/imageprocessor.h"
#include <QImage>
#include <QString>

namespace BackgroundPreview {
struct Result {QImage image,mask;QString error,notice;};
// 仅处理值数据，可由后台线程调用；不访问窗口、画笔控件或选择框。
Result render(const QImage &source,const ImageProcessor::Options &options,bool needsMask);
}
#endif
