#include "imagefiles.h"
#include <QFile>
#include <stdexcept>
QString ImageFiles::saveUniquePng(const QImage &image,QString path)
{
    if(image.isNull())throw std::runtime_error("图片为空，无法保存");
    if(!path.endsWith(".png",Qt::CaseInsensitive))path+=".png";
    const QString base=path.left(path.size()-4);
    QFile file;int number=1;
    for(;;) {
        file.setFileName(path);
        if(file.open(QIODevice::WriteOnly|QIODevice::NewOnly))break;
        if(!QFile::exists(path))throw std::runtime_error(("无法创建输出文件："+file.errorString()).toUtf8().constData());
        path=QString("%1(%2).png").arg(base).arg(number++);
    }
    if(!image.save(&file,"PNG") || !file.flush()) {
        file.close();const bool removed=file.remove();
        throw std::runtime_error(removed ? "图片写入失败，请检查磁盘空间与权限" : "图片写入失败，未能清除不完整文件");
    }
    file.close();return path;
}
