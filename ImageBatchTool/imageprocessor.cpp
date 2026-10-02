#include "imageprocessor.h"

#include <opencv2/imgproc.hpp>

cv::Mat ImageProcessor::toGrayscale(const cv::Mat &rgb)
{
    cv::Mat gray;
    cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);

    return gray;
}