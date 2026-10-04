#include "application/batchjob.h"
#include "infrastructure/imagefiles.h"
#include "application/imageprocessing.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <exception>

namespace {
class BatchWorker final : public QObject {
    Q_OBJECT
public:
    QString input,output;
    BatchProcessing::Parameters parameters;
    std::shared_ptr<std::atomic_bool> cancelled;
signals:
    void planned(int total);
    void itemFinished(BatchItem item,int done,int total);
    void finished(int total,int succeeded,int failed,bool cancelled,QString error);
public slots:
    void run() {
        int total=0,success=0,failed=0;QString fatal;
        try {
            const QString source=QFileInfo(input).canonicalFilePath();
            if(source.isEmpty() || !QFileInfo(source).isDir())throw std::runtime_error("输入文件夹不存在");
            if(!QDir().mkpath(output))throw std::runtime_error("无法创建输出文件夹");
            const QString destination=QFileInfo(output).canonicalFilePath();
#ifdef Q_OS_WIN
            const bool same=source.compare(destination,Qt::CaseInsensitive)==0;
#else
            const bool same=source==destination;
#endif
            if(same)throw std::runtime_error("输出文件夹必须与输入文件夹不同");
            QStringList files;
            for(const auto &file:QDir(source).entryInfoList(QDir::Files,QDir::Name))
                if(QStringList{"png","jpg","jpeg","bmp","webp","tif","tiff"}.contains(file.suffix().toLower()))files.append(file.absoluteFilePath());
            total=files.size();emit planned(total);
            if(total==0)throw std::runtime_error("输入文件夹没有支持的图片文件（不包含子文件夹）");
            for(const auto &path:files) {
                if(cancelled->load())break;
                BatchItem item;item.input=path;
                try {
                    QImageReader reader(path);reader.setAutoTransform(true);
                    const QImage original=reader.read();
                    if(original.isNull())throw std::runtime_error(("读取失败："+reader.errorString()).toUtf8().constData());
                    const auto options=parameters.forImage(cv::Size(original.width(),original.height()));
                    const auto image=ImageProcessing::processImage(original,options);
                    if(cancelled->load())break; // 当前处理完成后安全取消，不再写入。
                    item.output=ImageFiles::saveUniquePng(image,QDir(destination).filePath(QFileInfo(path).completeBaseName()+"-result.png"));
                    ++success;
                }catch(const std::exception &e){item.error=QString::fromUtf8(e.what());++failed;}
                emit itemFinished(item,success+failed,total);
            }
        }catch(const std::exception &e){fatal=QString::fromUtf8(e.what());}
        emit finished(total,success,failed,cancelled->load(),fatal);
    }
};
}
BatchJob::BatchJob(QObject *parent):QObject(parent){qRegisterMetaType<BatchItem>();}
BatchJob::~BatchJob() {
    cancel();
    if(thread_){thread_->quit();thread_->wait();}
}
void BatchJob::cancel(){if(cancel_)cancel_->store(true);}
bool BatchJob::start(const QString &input,const QString &output,const ImageProcessor::Options &options) {
    return start(input,output,BatchProcessing::Parameters::fromOptions(options));
}
bool BatchJob::start(const QString &input,const QString &output,const BatchProcessing::Parameters &parameters) {
    if(isRunning())return false;
    thread_=new QThread(this);cancel_=std::make_shared<std::atomic_bool>(false);
    auto *worker=new BatchWorker;worker->input=input;worker->output=output;worker->parameters=parameters;worker->cancelled=cancel_;
    worker->moveToThread(thread_);
    connect(thread_,&QThread::started,worker,&BatchWorker::run);
    connect(worker,&BatchWorker::planned,this,&BatchJob::planned);
    connect(worker,&BatchWorker::itemFinished,this,&BatchJob::itemFinished);
    connect(worker,&BatchWorker::finished,thread_,&QThread::quit,Qt::DirectConnection);
    connect(thread_,&QThread::finished,worker,&QObject::deleteLater);
    // 先等待线程退出，再通知界面，允许安全关闭或启动下一次任务。
    connect(worker,&BatchWorker::finished,this,[this](int total,int success,int failed,bool cancelled,QString error) {
        thread_->wait();thread_->deleteLater();thread_=nullptr;
        emit finished(total,success,failed,cancelled,error);
    });
    thread_->start();return true;
}
#include "batchjob.moc"
