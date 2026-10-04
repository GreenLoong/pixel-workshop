#include "application/imagetask.h"
#include <QtConcurrent>
#include <exception>
#include <stdexcept>

ImageTask::ImageTask(QObject *parent,Processor processor):QObject(parent),processor_(std::move(processor))
{
    connect(&watcher_,&QFutureWatcher<Result>::finished,this,[this] {
        const auto result=watcher_.result();
        active_=false;
        if(pending_) {
            auto request=std::move(*pending_);pending_.reset();launch(std::move(request));return;
        }
        const bool current=result.generation==generation_;
        setBusy(false);
        if(current)emit completed(result.image,result.error);
    });
}
void ImageTask::setBusy(bool busy)
{
    if(busy_==busy)return;
    busy_=busy;emit busyChanged(busy);
}
void ImageTask::submit(const QImage &image,const ImageProcessor::Options &options)
{
    Request request{image,options,++generation_};setBusy(true);
    // active 在完成槽中清除，避免计算已结束但通知尚未到达的竞态。
    if(active_)pending_=std::move(request);
    else launch(std::move(request));
}
void ImageTask::cancel()
{
    ++generation_;pending_.reset();setBusy(false);
}
void ImageTask::launch(Request request)
{
    const auto processor=processor_;
    active_=true;
    watcher_.setFuture(QtConcurrent::run([request=std::move(request),processor] {
        Result result{{},{},request.generation};
        try {
            result.image=processor(request.image,request.options);
            if(result.image.isNull())throw std::runtime_error("无法生成处理结果");
        }catch(const std::exception &error){result.error=QString::fromUtf8(error.what());}
        catch(...){result.error="图像处理发生未知错误";}
        return result;
    }));
}
