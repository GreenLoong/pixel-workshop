#ifndef BATCHJOB_H
#define BATCHJOB_H
#include "imageprocessor.h"
#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <memory>
struct BatchItem {QString input,output,error;};
Q_DECLARE_METATYPE(BatchItem)
// 生命周期由界面持有；只有工作线程进行读取、处理和写入。
class BatchJob final : public QObject {
    Q_OBJECT
public:
    explicit BatchJob(QObject *parent=nullptr);
    ~BatchJob() override;
    bool start(const QString &input,const QString &output,const ImageProcessor::Options &options);
    void cancel();
    bool isRunning() const {return thread_!=nullptr;}
signals:
    void planned(int total);
    void itemFinished(BatchItem item,int done,int total);
    void finished(int total,int succeeded,int failed,bool cancelled,QString error);
private:
    QThread *thread_=nullptr;
    std::shared_ptr<std::atomic_bool> cancel_;
};
#endif
