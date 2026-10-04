#pragma once
#include "domain/imageprocessor.h"
namespace BatchProcessing {
enum class SizeMode { Original, Exact, LongEdge, Percent };
struct Parameters {
    ImageProcessor::Options processing;
    SizeMode sizeMode=SizeMode::Original;
    int longEdge=1200;
    double percent=100;
    bool allowUpscale=false;
    static Parameters fromOptions(const ImageProcessor::Options &options);
    ImageProcessor::Options forImage(cv::Size original) const;
};
}
