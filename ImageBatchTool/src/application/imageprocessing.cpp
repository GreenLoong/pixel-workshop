#include "application/imageprocessing.h"

#include <stdexcept>
#include "infrastructure/modelresources.h"
#include "infrastructure/imageconversion.h"

QImage ImageProcessing::processImage(const QImage &original, const ImageProcessor::Options &options)
{
    if (original.isNull())
        throw std::runtime_error("The original image is empty");
    const bool alpha = original.hasAlphaChannel();
    const QImage rgb = original.convertToFormat(alpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
    if (rgb.isNull())
        throw std::runtime_error("Unable to convert the original image");
    const cv::Mat source=ImageConversion::rgbView(rgb);
    auto processing=options;
    if(processing.background!=ImageProcessor::BackgroundMode::None
        && processing.segmentation==ImageProcessor::SegmentationMethod::Human)
        processing.humanModel=ModelResources::humanModel();
    const cv::Mat result = ImageProcessor::process(source, processing);
    return ImageConversion::copyImage(result);
}
