// PPHumanSeg inference adapted from OpenCV Zoo's pphumanseg.py.
// Copyright (C) 2021, Shenzhen Institute of Artificial Intelligence and Robotics for Society.
// Copyright (c) 2021 PaddlePaddle Authors. Apache-2.0; see models/PPHumanSeg-LICENSE.txt.
// Changes: C++ RGB input, per-thread model reuse, full-size probability comparison.
#include "domain/humansegmentation.h"
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

cv::Mat ImageProcessor::segmentHuman(const cv::Mat &rgb,
    const std::shared_ptr<const std::vector<uchar>> &model)
{
    if(rgb.empty() || rgb.type()!=CV_8UC3 || !model || model->empty())
        CV_Error(cv::Error::StsBadArg,"Human segmentation requires RGB input and model bytes");
    // OpenCV Net 的 setInput/forward 改变内部状态，每个线程持有独立实例。
    struct Session {std::shared_ptr<const std::vector<uchar>> weights;cv::dnn::Net net;};
    thread_local Session session;
    if(session.weights!=model) {
        cv::dnn::Net net=cv::dnn::readNetFromONNX(*model);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        session.net=std::move(net);session.weights=model;
    }
    // 原图已经是 RGB，不能再次交换红蓝通道。与上游归一化公式一致。
    const auto blob=cv::dnn::blobFromImage(rgb,1.0/127.5,cv::Size(192,192),cv::Scalar(127.5,127.5,127.5),false,false);
    session.net.setInput(blob);
    const cv::Mat output=session.net.forward();
    if(output.dims!=4 || output.type()!=CV_32F || output.size[0]!=1 || output.size[1]!=2
        || output.size[2]!=192 || output.size[3]!=192)
        CV_Error(cv::Error::StsError,"Unexpected PPHumanSeg output shape");
    const cv::Mat background(192,192,CV_32F,const_cast<float*>(output.ptr<float>(0,0)));
    const cv::Mat foreground(192,192,CV_32F,const_cast<float*>(output.ptr<float>(0,1)));
    cv::Mat fg,bg;
    cv::resize(foreground,fg,rgb.size(),0,0,cv::INTER_LINEAR);
    cv::resize(background,bg,rgb.size(),0,0,cv::INTER_LINEAR);
    // 与官方 Python 示例一致：先还原两类概率，再选择概率较大的类别。
    return fg>bg;
}
