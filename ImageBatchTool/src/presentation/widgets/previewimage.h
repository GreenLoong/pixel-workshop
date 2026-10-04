#ifndef PREVIEWIMAGE_H
#define PREVIEWIMAGE_H

#include <QImage>

namespace PreviewImage {
inline constexpr int MaxExtent=1280;
inline QSize boundedSize(QSize actual,int limit=MaxExtent)
{
    return actual.scaled(limit,limit,Qt::KeepAspectRatio).boundedTo(actual);
}
inline QImage thumbnail(const QImage &original,int limit=MaxExtent)
{
    return original.width()>limit || original.height()>limit
        ?original.scaled(limit,limit,Qt::KeepAspectRatio,Qt::SmoothTransformation):original;
}
}
#endif
