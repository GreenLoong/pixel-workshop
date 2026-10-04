#include "batchjob.h"
#include "imagefiles.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
QByteArray hash(const QString &path){QFile f(path);f.open(QIODevice::ReadOnly);return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256);}
struct Result {int total=0,success=0,failed=0,ticks=0;bool cancelled=false,timedOut=false,guiThread=true;QString error;QList<BatchItem> items;};
Result run(BatchJob &job,const QString &input,const QString &output,const ImageProcessor::Options &options,bool cancelImmediately=false,bool cancelOnFirst=false) {
    Result result;QEventLoop loop;QTimer heartbeat,timeout;
    QObject::connect(&heartbeat,&QTimer::timeout,&loop,[&]{++result.ticks;});
    QObject::connect(&timeout,&QTimer::timeout,&loop,[&]{result.timedOut=true;job.cancel();});
    QObject::connect(&job,&BatchJob::itemFinished,&loop,[&](BatchItem item,int,int) {
        result.guiThread &= QThread::currentThread()==QApplication::instance()->thread();
        result.items.append(item);if(cancelOnFirst)job.cancel();
    });
    QObject::connect(&job,&BatchJob::finished,&loop,[&](int total,int success,int failed,bool cancelled,QString error) {
        result.total=total;result.success=success;result.failed=failed;result.cancelled=cancelled;result.error=error;loop.quit();
    });
    require(job.start(input,output,options),"Could not start worker");
    require(!job.start(input,output,options),"Allowed concurrent runs on one job");
    if(cancelImmediately)job.cancel();
    heartbeat.start(1);timeout.setSingleShot(true);timeout.start(10000);loop.exec();
    require(!result.timedOut,"Batch timeout");require(!job.isRunning(),"Thread still active after finished");return result;
}
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    try {
        QTemporaryDir temp(QDir::currentPath()+"/batch-tests-XXXXXX");require(temp.isValid(),"Temp folder failed");
        const QString input=temp.path()+"/输入图片",output=temp.path()+"/处理结果";
        QDir().mkpath(input+"/subfolder");
        QImage rgba(512,320,QImage::Format_RGBA8888);rgba.fill(QColor(255,0,0,77));
        require(rgba.save(input+"/中文.png"),"Fixture save failed");
        QImage rgb(512,320,QImage::Format_RGB888);rgb.fill(QColor(20,80,120));require(rgb.save(input+"/sample.PNG"),"Uppercase fixture failed");
        rgb.save(input+"/subfolder/ignored.png");
        QFile broken(input+"/broken.png");broken.open(QIODevice::WriteOnly);broken.write("damaged");broken.close();
        const QByteArray originalHash=hash(input+"/中文.png");
        ImageProcessor::Options options;options.grayscale=true;options.exposure=.5;options.targetSize=cv::Size(256,160);
        BatchJob job;
        auto first=run(job,input,output,options);
        require(first.total==3 && first.success==2 && first.failed==1 && first.items.size()==3,"Counts or per-image failure handling wrong");
        require(first.error.isEmpty() && first.guiThread && first.ticks>0,"Main event loop blocked or worker error");
        QImage processed(output+"/中文-result.png");
        require(processed.size()==QSize(256,160) && processed.pixelColor(0,0).alpha()==77,"Unicode output, size or alpha incorrect");
        require(processed.pixelColor(0,0).red()==processed.pixelColor(0,0).green(),"Combined gray processing failed");
        auto second=run(job,input,output,options);
        require(second.success==2 && QFile::exists(output+"/中文-result(1).png"),"Repeated batch overwrote existing files");
        require(hash(input+"/中文.png")==originalHash,"Batch modified original");
        auto same=run(job,input,input,options);
        require(!same.error.isEmpty() && same.success==0,"Same source and destination accepted");
        auto cancelled=run(job,input,temp.path()+"/cancelled",options,true);
        require(cancelled.cancelled && cancelled.success==0,"Immediate cancellation wrote files");
        const QString many=temp.path()+"/many";QDir().mkpath(many);
        for(int i=0;i<16;++i)rgb.save(many+QString("/%1.png").arg(i,2,10,QChar('0')));
        ImageProcessor::Options slow;slow.clarity=30;slow.exposure=.5;
        auto mid=run(job,many,temp.path()+"/cancel-mid",slow,false,true);
        require(mid.cancelled && mid.success>=1 && mid.success<mid.total,"Mid-task cancellation did not stop remaining files");
        QFile blocker(temp.path()+"/blocker");blocker.open(QIODevice::WriteOnly);blocker.close();
        auto failed=run(job,input,blocker.fileName(),options);
        require(!failed.error.isEmpty() && failed.success==0,"Invalid output directory was accepted");
        const QString unique=ImageFiles::saveUniquePng(rgba,temp.path()+"/result.png");
        require(ImageFiles::saveUniquePng(rgba,unique).endsWith("result(1).png"),"Shared single-image save lacks collision protection");
        if(argc>1) {
            QImage portrait(QString::fromLocal8Bit(argv[1]));require(!portrait.isNull(),"Portrait fixture unreadable");
            const QString humans=temp.path()+"/portraits",humanOutput=temp.path()+"/human-output";
            QDir().mkpath(humans);portrait.save(humans+"/person1.png");portrait.save(humans+"/person2.png");
            ImageProcessor::Options removal;removal.background=ImageProcessor::BackgroundMode::Remove;
            const auto humanResult=run(job,humans,humanOutput,removal);
            require(humanResult.success==2 && humanResult.failed==0,"Human model batch failed");
            const QImage output(humanOutput+"/person1-result.png");
            require(output.pixelColor(output.width()/2,output.height()*.9).alpha()>240
                && output.pixelColor(2,2).alpha()<10,"Human batch removed clothing or retained background");
            std::cout<<"PASS: portrait segmentation in background batch worker\n";
        }
        std::cout<<"PASS: background worker, responsive UI, Unicode paths, mixed failures, alpha, original protection, repeat runs and cancellation\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
