#include "imageprocessor.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

cv::Mat ImageProcessor::processBackground(const cv::Mat &source, const Options &o)
{
    if(o.background == BackgroundMode::None) return source;
    if(o.feather<0 || o.feather>20 || o.backgroundBlur<1 || o.backgroundBlur>50)
        CV_Error(cv::Error::StsBadArg,"Invalid background parameters");
    cv::Mat rgb;
    if(source.channels()==4)cv::cvtColor(source,rgb,cv::COLOR_RGBA2RGB); else rgb=source;
    const double scale=std::min(1.0,1024.0/std::max(rgb.cols,rgb.rows));
    cv::Mat small;
    cv::resize(rgb,small,cv::Size(std::max(2,cvRound(rgb.cols*scale)),std::max(2,cvRound(rgb.rows*scale))));
    const auto &r=o.foregroundRect;
    if(!std::isfinite(r.x+r.y+r.width+r.height) || r.x<0 || r.y<0 || r.width<=0 || r.height<=0
        || r.x+r.width>1 || r.y+r.height>1 || small.rows<3 || small.cols<3)
        CV_Error(cv::Error::StsBadArg,"Invalid foreground region or image too small");
    cv::Mat mask(small.size(),CV_8UC1,cv::Scalar(cv::GC_BGD));
    const int x=std::clamp(cvRound(r.x*small.cols),0,small.cols-2);
    const int y=std::clamp(cvRound(r.y*small.rows),0,small.rows-2);
    const int w=std::clamp(cvRound(r.width*small.cols),1,small.cols-x);
    const int h=std::clamp(cvRound(r.height*small.rows),1,small.rows-y);
    mask(cv::Rect(x,y,w,h)).setTo(cv::GC_PR_FGD);
    // 笔触是明确的前景／背景标记；线段连续填充，快速拖动也不会留下间隙。
    for(const auto &stroke:o.strokes) {
        if(!std::isfinite(stroke.radius) || stroke.radius<=0 || stroke.radius>0.5)
            CV_Error(cv::Error::StsBadArg,"Invalid brush radius");
        const int radius=std::max(1,cvRound(stroke.radius*std::min(small.cols,small.rows)));
        cv::Point previous;
        bool first=true;
        for(const auto &p:stroke.points) {
            if(!std::isfinite(p.x+p.y) || p.x<0 || p.x>1 || p.y<0 || p.y>1)
                CV_Error(cv::Error::StsBadArg,"Invalid brush point");
            cv::Point point(cvRound(p.x*(small.cols-1)),cvRound(p.y*(small.rows-1)));
            const cv::Scalar label(stroke.foreground ? cv::GC_FGD : cv::GC_BGD);
            if(!first)cv::line(mask,previous,point,label,2*radius,cv::LINE_8);
            cv::circle(mask,point,radius,label,-1);
            previous=point; first=false;
        }
    }
    if(cv::countNonZero(mask==cv::GC_BGD)<5 || cv::countNonZero((mask==cv::GC_PR_FGD)|(mask==cv::GC_FGD))<5)
        CV_Error(cv::Error::StsBadArg,"Keep both foreground and background samples in the region/brush mask");
    cv::Mat bgModel,fgModel;
    cv::grabCut(small,mask,cv::Rect(),bgModel,fgModel,3,cv::GC_INIT_WITH_MASK);
    cv::Mat alpha=(mask==cv::GC_FGD)|(mask==cv::GC_PR_FGD);
    if(o.feather)cv::GaussianBlur(alpha,alpha,cv::Size(),o.feather);
    cv::resize(alpha,alpha,source.size(),0,0,cv::INTER_LINEAR);
    if(source.channels()==4) {
        cv::Mat existing;cv::extractChannel(source,existing,3);
        cv::multiply(alpha,existing,alpha,1.0/255);
    }
    if(o.background==BackgroundMode::Remove) {
        cv::Mat result;cv::cvtColor(rgb,result,cv::COLOR_RGB2RGBA);
        cv::insertChannel(alpha,result,3);return result;
    }
    cv::Mat background;
    if(o.background==BackgroundMode::Blur)
        cv::GaussianBlur(rgb,background,cv::Size(),o.backgroundBlur/std::max(scale,0.001));
    else if(o.background==BackgroundMode::Replace) {
        if(o.backgroundImage.empty())background=cv::Mat(source.size(),CV_8UC3,o.backgroundColor);
        else {
            if(o.backgroundImage.type()!=CV_8UC3)CV_Error(cv::Error::StsBadArg,"Replacement image must be RGB");
            cv::resize(o.backgroundImage,background,source.size());
        }
    } else CV_Error(cv::Error::StsBadArg,"Unknown background mode");
    cv::Mat result(rgb.size(),CV_8UC3);
    for(int row=0;row<rgb.rows;++row)for(int col=0;col<rgb.cols;++col) {
        const float a=alpha.at<uchar>(row,col)/255.f;
        const auto f=rgb.at<cv::Vec3b>(row,col),b=background.at<cv::Vec3b>(row,col);
        auto &p=result.at<cv::Vec3b>(row,col);
        for(int c=0;c<3;++c)p[c]=cv::saturate_cast<uchar>(f[c]*a+b[c]*(1-a));
    }
    return result;
}
