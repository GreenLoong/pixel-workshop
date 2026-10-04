#include "domain/imageprocessor.h"
#include "domain/humansegmentation.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace {
cv::Size segmentationSize(cv::Size size)
{
    const double scale=std::min(1.0,1024.0/std::max(size.width,size.height));
    return cv::Size(std::max(2,cvRound(size.width*scale)),std::max(2,cvRound(size.height*scale)));
}
void applyStrokes(cv::Mat &mask,const std::vector<ImageProcessor::BrushStroke> &strokes,bool grabCutLabels)
{
    for(const auto &stroke:strokes) {
        if(!std::isfinite(stroke.radius) || stroke.radius<=0 || stroke.radius>0.5)
            CV_Error(cv::Error::StsBadArg,"Invalid brush radius");
        const int radius=std::max(1,cvRound(stroke.radius*std::min(mask.cols,mask.rows)));
        cv::Point previous;bool first=true;
        for(const auto &p:stroke.points) {
            if(!std::isfinite(p.x+p.y) || p.x<0 || p.x>1 || p.y<0 || p.y>1)
                CV_Error(cv::Error::StsBadArg,"Invalid brush point");
            const cv::Point point(cvRound(p.x*(mask.cols-1)),cvRound(p.y*(mask.rows-1)));
            const cv::Scalar label(grabCutLabels ? (stroke.foreground?cv::GC_FGD:cv::GC_BGD) : (stroke.foreground?255:0));
            if(!first)cv::line(mask,previous,point,label,2*radius,cv::LINE_8);
            cv::circle(mask,point,radius,label,-1);previous=point;first=false;
        }
    }
}
cv::Mat regionMask(cv::Size size,const ImageProcessor::Options &o)
{
    const auto &r=o.foregroundRect;
    if(!std::isfinite(r.x+r.y+r.width+r.height) || r.x<0 || r.y<0 || r.width<=0 || r.height<=0
        || r.x+r.width>1.000001 || r.y+r.height>1.000001 || size.empty())
        CV_Error(cv::Error::StsBadArg,"Invalid foreground region");
    cv::Mat mask(size,CV_8UC1,cv::Scalar(cv::GC_BGD));
    const int x=std::clamp(cvRound(r.x*size.width),0,size.width-1);
    const int y=std::clamp(cvRound(r.y*size.height),0,size.height-1);
    const int w=std::clamp(cvRound(r.width*size.width),1,size.width-x);
    const int h=std::clamp(cvRound(r.height*size.height),1,size.height-y);
    mask(cv::Rect(x,y,w,h)).setTo(cv::GC_PR_FGD);
    applyStrokes(mask,o.strokes,true);
    return mask;
}
ImageProcessor::RegionSamples countSamples(const cv::Mat &mask)
{
    return {cv::countNonZero((mask==cv::GC_PR_FGD)|(mask==cv::GC_FGD)),cv::countNonZero(mask==cv::GC_BGD)};
}
cv::Mat segmentRegion(const cv::Mat &rgb,const ImageProcessor::Options &o)
{
    cv::Mat mask=regionMask(rgb.size(),o);
    // 全选、极小范围或画笔覆盖整类样本时，无法初始化 GrabCut 的两组模型。
    // 保留用户的手工标记，不凭空添加背景点，也不将可继续编辑的状态当成异常。
    if(!countSamples(mask).canSegment())return (mask==cv::GC_FGD)|(mask==cv::GC_PR_FGD);
    cv::Mat bgModel,fgModel;cv::grabCut(rgb,mask,cv::Rect(),bgModel,fgModel,3,cv::GC_INIT_WITH_MASK);
    return (mask==cv::GC_FGD)|(mask==cv::GC_PR_FGD);
}
}
ImageProcessor::RegionSamples ImageProcessor::regionSamples(cv::Size imageSize,const Options &options)
{
    if(imageSize.empty())CV_Error(cv::Error::StsBadArg,"Empty image");
    return countSamples(regionMask(segmentationSize(imageSize),options));
}
cv::Mat ImageProcessor::processBackground(const cv::Mat &source,const Options &o)
{
    if(o.background==BackgroundMode::None)return source;
    if(o.feather<0 || o.feather>20 || o.backgroundBlur<1 || o.backgroundBlur>50)
        CV_Error(cv::Error::StsBadArg,"Invalid background parameters");
    cv::Mat rgb;
    if(source.channels()==4)cv::cvtColor(source,rgb,cv::COLOR_RGBA2RGB);else rgb=source;
    const double scale=std::min(1.0,1024.0/std::max(rgb.cols,rgb.rows));
    cv::Mat small;cv::resize(rgb,small,segmentationSize(rgb.size()));
    cv::Mat alpha;
    if(o.segmentation==SegmentationMethod::Human) {
        alpha=segmentHuman(small,o.humanModel);
        // 画笔直接修改模型蒙版，不再由颜色聚类重新推翻人工标记。
        applyStrokes(alpha,o.strokes,false);
    }else if(o.segmentation==SegmentationMethod::Region)alpha=segmentRegion(small,o);
    else CV_Error(cv::Error::StsBadArg,"Unknown segmentation method");
    if(o.feather)cv::GaussianBlur(alpha,alpha,cv::Size(),o.feather);
    cv::resize(alpha,alpha,source.size(),0,0,cv::INTER_LINEAR);
    if(source.channels()==4) {
        cv::Mat existing;cv::extractChannel(source,existing,3);cv::multiply(alpha,existing,alpha,1.0/255);
    }
    if(o.background==BackgroundMode::Remove) {
        cv::Mat result;cv::cvtColor(rgb,result,cv::COLOR_RGB2RGBA);cv::insertChannel(alpha,result,3);return result;
    }
    cv::Mat background;
    if(o.background==BackgroundMode::Blur)cv::GaussianBlur(rgb,background,cv::Size(),o.backgroundBlur/std::max(scale,0.001));
    else if(o.background==BackgroundMode::Replace) {
        if(o.backgroundImage.empty())background=cv::Mat(source.size(),CV_8UC3,o.backgroundColor);
        else {
            if(o.backgroundImage.type()!=CV_8UC3)CV_Error(cv::Error::StsBadArg,"Replacement image must be RGB");
            cv::resize(o.backgroundImage,background,source.size());
        }
    }else CV_Error(cv::Error::StsBadArg,"Unknown background mode");
    cv::Mat result(rgb.size(),CV_8UC3);
    for(int row=0;row<rgb.rows;++row)for(int col=0;col<rgb.cols;++col) {
        const float a=alpha.at<uchar>(row,col)/255.f;
        const auto f=rgb.at<cv::Vec3b>(row,col),b=background.at<cv::Vec3b>(row,col);
        auto &p=result.at<cv::Vec3b>(row,col);
        for(int c=0;c<3;++c)p[c]=cv::saturate_cast<uchar>(f[c]*a+b[c]*(1-a));
    }
    return result;
}
