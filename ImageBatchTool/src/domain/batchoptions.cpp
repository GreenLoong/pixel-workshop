#include "domain/batchoptions.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
using namespace BatchProcessing;
Parameters Parameters::fromOptions(const ImageProcessor::Options &options)
{
    Parameters result;result.processing=options;
    if(options.targetSize!=cv::Size())result.sizeMode=SizeMode::Exact;
    return result;
}
ImageProcessor::Options Parameters::forImage(cv::Size original) const
{
    auto result=processing;
    if(sizeMode==SizeMode::Original) {result.targetSize={};return result;}
    if(sizeMode==SizeMode::Exact) {
        if(!ImageProcessor::validOutputSize(result.targetSize))throw std::runtime_error("批量固定尺寸超出允许范围");
        return result;
    }
    const auto size=ImageProcessor::geometrySize(original,result);
    double scale=1;
    if(sizeMode==SizeMode::LongEdge) {
        if(longEdge<1 || longEdge>20000)throw std::runtime_error("长边应在 1～20000 像素之间");
        scale=static_cast<double>(longEdge)/std::max(size.width,size.height);
        if(!allowUpscale)scale=std::min(1.0,scale);
    }else if(sizeMode==SizeMode::Percent) {
        if(!std::isfinite(percent) || percent<1 || percent>800)throw std::runtime_error("缩放百分比应在 1～800 之间");
        scale=percent/100;
    }else throw std::runtime_error("未知批量尺寸模式");
    result.targetSize=cv::Size(std::max(1,static_cast<int>(std::lround(size.width*scale))),
                              std::max(1,static_cast<int>(std::lround(size.height*scale))));
    if(!ImageProcessor::validOutputSize(result.targetSize))throw std::runtime_error("处理结果不能超过 4000 万像素");
    return result;
}
