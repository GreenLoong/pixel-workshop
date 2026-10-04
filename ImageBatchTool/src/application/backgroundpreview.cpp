#include "application/backgroundpreview.h"
#include "application/imageprocessing.h"

#include <exception>

BackgroundPreview::Result BackgroundPreview::render(const QImage &source,
    const ImageProcessor::Options &options,bool needsMask)
{
    Result result;
    try {
        if(options.segmentation==ImageProcessor::SegmentationMethod::Region
            && (options.background!=ImageProcessor::BackgroundMode::None || needsMask)) {
            const auto samples=ImageProcessor::regionSamples(cv::Size(source.width(),source.height()),options);
            if(samples.background==0)
                result.notice="当前没有背景标记，已保留整张图片。请缩小主体范围，或用“删除背景”画笔标出一部分背景。";
            else if(samples.foreground==0)
                result.notice="主体已被全部标为背景。可撤销笔触，或用“保留主体”画笔补回需要保留的区域。";
            else if(!samples.canSegment())
                result.notice="当前范围或画笔标记太少，暂按手工蒙版预览。请扩大主体范围，或补画主体和背景后继续自动识别。";
        }
        result.image=ImageProcessing::processImage(source,options);
        if(options.background==ImageProcessor::BackgroundMode::Remove)result.mask=result.image;
        else if(needsMask) {
            auto maskOptions=options;
            maskOptions.background=ImageProcessor::BackgroundMode::Remove;
            maskOptions.clearTone();maskOptions.grayscale=false;
            result.mask=ImageProcessing::processImage(source,maskOptions);
        }
    } catch(const std::exception &e) {result.error=QString::fromUtf8(e.what());}
    return result;
}
