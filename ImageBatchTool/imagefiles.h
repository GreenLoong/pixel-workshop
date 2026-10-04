#ifndef IMAGEFILES_H
#define IMAGEFILES_H
#include <QImage>
#include <QString>
namespace ImageFiles {
// 创建新文件，同名自动编号；失败时删除本次创建的不完整文件。
QString saveUniquePng(const QImage &image, QString requestedPath);
}
#endif
