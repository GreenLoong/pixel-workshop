#include "infrastructure/modelresources.h"
#include <QFile>
#include <QCryptographicHash>
#include <stdexcept>

std::shared_ptr<const std::vector<unsigned char>> ModelResources::humanModel()
{
    static const auto bytes=[] {
        QFile file(":/models/human_segmentation_pphumanseg_2023mar.onnx");
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("无法加载内置人像模型，请重新构建程序");
        const QByteArray data=file.readAll();
        if(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()
            != "552d8a984054e59b5d773d24b9b12022b22046ceb2bbc4c9aaeaceb36a9ddf24")
            throw std::runtime_error("人像模型校验失败，请重新构建程序");
        return std::make_shared<const std::vector<uchar>>(data.constData(),data.constData()+data.size());
    }();
    return bytes;
}
