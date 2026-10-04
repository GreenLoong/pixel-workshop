#ifndef HUMANSEGMENTATION_H
#define HUMANSEGMENTATION_H
#include <opencv2/core.hpp>
#include <memory>
#include <vector>
namespace ImageProcessor {
// PPHumanSeg 192×192 RGB 输入，返回同输入尺寸的 8 位前景蒙版。
cv::Mat segmentHuman(const cv::Mat &rgb, const std::shared_ptr<const std::vector<uchar>> &model);
}
#endif
