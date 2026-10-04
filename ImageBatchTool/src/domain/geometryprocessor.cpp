#include "domain/imageprocessor.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace {
cv::Size rotatedSize(cv::Size size,double angle)
{
    if(size.empty() || !std::isfinite(angle) || angle < -180 || angle > 180)
        CV_Error(cv::Error::StsBadArg,"Invalid rotation");
    if(angle==90 || angle==-90)return cv::Size(size.height,size.width);
    if(angle==0 || std::abs(angle)==180)return size;
    const double radians=angle*CV_PI/180.0;
    const cv::Size canvas(static_cast<int>(std::ceil(std::abs(size.width*std::cos(radians))
                                                  +std::abs(size.height*std::sin(radians)))),
                          static_cast<int>(std::ceil(std::abs(size.height*std::cos(radians))
                                                  +std::abs(size.width*std::sin(radians)))));
    if(!ImageProcessor::validOutputSize(canvas))
        CV_Error(cv::Error::StsBadArg,"Rotated canvas exceeds 40 million pixels");
    return canvas;
}

cv::Rect cropRect(cv::Size size,const cv::Rect2d &crop)
{
    if(crop==cv::Rect2d())return cv::Rect(0,0,size.width,size.height);
    if(!std::isfinite(crop.x) || !std::isfinite(crop.y) || !std::isfinite(crop.width)
        || !std::isfinite(crop.height) || crop.x<0 || crop.y<0 || crop.width<=0 || crop.height<=0
        || crop.x+crop.width>1.000001 || crop.y+crop.height>1.000001)
        CV_Error(cv::Error::StsBadArg,"Invalid normalized crop");
    const int x=std::clamp(static_cast<int>(std::floor(crop.x*size.width)),0,size.width-1);
    const int y=std::clamp(static_cast<int>(std::floor(crop.y*size.height)),0,size.height-1);
    const int right=std::clamp(static_cast<int>(std::ceil((crop.x+crop.width)*size.width)),x+1,size.width);
    const int bottom=std::clamp(static_cast<int>(std::ceil((crop.y+crop.height)*size.height)),y+1,size.height);
    return cv::Rect(x,y,right-x,bottom-y);
}
}

cv::Size ImageProcessor::geometrySize(cv::Size original,const Options &options)
{
    return cropRect(rotatedSize(original,options.rotation),options.crop).size();
}

cv::Mat ImageProcessor::transformGeometry(const cv::Mat &source,const Options &options)
{
    const cv::Size canvas=rotatedSize(source.size(),options.rotation);
    cv::Mat result=source.clone();
    if(options.flipHorizontal || options.flipVertical)
        cv::flip(source,result,options.flipHorizontal && options.flipVertical?-1:options.flipHorizontal?1:0);
    const double angle=options.rotation;
    if(angle==90 || angle==-90 || std::abs(angle)==180) {
        cv::rotate(result,result,std::abs(angle)==180?cv::ROTATE_180
            :angle==90?cv::ROTATE_90_CLOCKWISE:cv::ROTATE_90_COUNTERCLOCKWISE);
    } else if(angle!=0) {
        auto matrix=cv::getRotationMatrix2D(cv::Point2f((result.cols-1)/2.0f,(result.rows-1)/2.0f),-angle,1);
        matrix.at<double>(0,2)+=(canvas.width-result.cols)/2.0;
        matrix.at<double>(1,2)+=(canvas.height-result.rows)/2.0;
        cv::Mat rotated;
        cv::warpAffine(result,rotated,matrix,canvas,cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar(255,255,255,0));
        result=rotated;
    }
    if(options.crop!=cv::Rect2d())result=result(cropRect(result.size(),options.crop)).clone();
    return result;
}
