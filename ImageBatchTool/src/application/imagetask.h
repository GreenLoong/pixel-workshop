#pragma once
#include "application/imageprocessing.h"
#include <QObject>
#include <QFutureWatcher>
#include <functional>
#include <optional>

// 一个正在计算的任务和一份最新请求；后台只持有值，不接触窗口或 QPixmap。
class ImageTask final : public QObject {
    Q_OBJECT
public:
    using Processor = std::function<QImage(const QImage &, const ImageProcessor::Options &)>;
    explicit ImageTask(QObject *parent=nullptr, Processor processor=ImageProcessing::processImage);
    void submit(const QImage &image,const ImageProcessor::Options &options);
    void cancel(); // 丢弃结果，不能强行中断正在执行的 OpenCV 调用。
    bool isBusy() const {return busy_;}
signals:
    void completed(const QImage &image,const QString &error);
    void busyChanged(bool busy);
private:
    struct Request {QImage image;ImageProcessor::Options options;quint64 generation;};
    struct Result {QImage image;QString error;quint64 generation;};
    void launch(Request request);
    void setBusy(bool busy);
    Processor processor_;
    QFutureWatcher<Result> watcher_;
    std::optional<Request> pending_;
    quint64 generation_=0;
    bool busy_=false;
    bool active_=false;
};
