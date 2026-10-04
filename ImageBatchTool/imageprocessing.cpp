#include "imageprocessing.h"

#include <stdexcept>
#include <QFile>
#include <QCryptographicHash>

namespace {
std::shared_ptr<const std::vector<uchar>> humanModel()
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
}

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
    auto processing=options;
    if(processing.background!=ImageProcessor::BackgroundMode::None
        && processing.segmentation==ImageProcessor::SegmentationMethod::Human)
        processing.humanModel=humanModel();
    const cv::Mat result = ImageProcessor::process(source, processing);
    const QImage::Format format = result.channels() == 1
        ? QImage::Format_Grayscale8 : result.channels()==4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    // 结果复制为 Qt 自己管理的内存，避免 cv::Mat 析构后留下悬空指针。
    const QImage output = QImage(result.data, result.cols, result.rows,
                                static_cast<qsizetype>(result.step), format).copy();
    if (output.isNull())
        throw std::runtime_error("Unable to create the output image");
    return output;
}
