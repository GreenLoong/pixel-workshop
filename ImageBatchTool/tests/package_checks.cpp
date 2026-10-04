#include "application/imagetask.h"
#include "application/batchjob.h"
#include "application/imageprocessing.h"
#include "infrastructure/imagefiles.h"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QImageReader>
#include <QEventLoop>
#include <QTimer>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        QTemporaryDir temp(QDir::currentPath()+"/package-checks-XXXXXX");require(temp.isValid(),"Cannot create test folder");
        const QString input=temp.filePath("中文图片"),output=temp.filePath("中文输出");require(QDir().mkpath(input),"Cannot create Unicode directory");
        QImage source(2048,1536,QImage::Format_RGBA8888);source.fill(QColor(255,0,0,77));
        const auto path=input+"/原图.png";require(source.save(path),"PNG writer/plugin failed");
        QImageReader reader(path);const auto read=reader.read();
        require(read.convertToFormat(source.format())==source,"PNG read roundtrip failed");
        const auto jpegPath=temp.filePath("格式检查.jpg");
        require(source.convertToFormat(QImage::Format_RGB888).save(jpegPath),"JPEG writer/plugin failed");
        const QImage jpeg(jpegPath);
        require(!jpeg.isNull() && jpeg.size()==source.size() && jpeg.pixelColor(0,0).red()>=250,"JPEG reader/plugin failed");
        ImageTask task;QEventLoop loop;QTimer timeout;bool finished=false;QImage result;QString processingError;
        ImageProcessor::Options options;options.grayscale=true;options.brightness=15;options.targetSize={1024,768};
        QObject::connect(&task,&ImageTask::completed,&loop,[&](const QImage &image,const QString &error){processingError=error;result=image;finished=true;loop.quit();});
        QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);timeout.setSingleShot(true);timeout.start(15000);
        task.submit(read,options);loop.exec();require(finished,"Image processing timeout");
        require(processingError.isEmpty(),"Async pipeline failed");
        require(result.size()==QSize(1024,768) && result.pixelColor(0,0)==QColor(91,91,91,77),"Combined processing pixels/alpha incorrect");
        const auto saved=ImageFiles::saveUniquePng(result,temp.filePath("结果.png"));
        require(ImageFiles::saveUniquePng(result,saved).endsWith("结果(1).png"),"Collision protection failed");
        QFile bad(input+"/坏图.png");require(bad.open(QIODevice::WriteOnly),"Cannot create damaged fixture");bad.write("broken");bad.close();
        BatchJob batch;int success=-1,failed=-1;QString fatal;bool done=false;
        QObject::connect(&batch,&BatchJob::finished,&loop,[&](int,int s,int f,bool,QString e){success=s;failed=f;fatal=e;done=true;loop.quit();});
        BatchProcessing::Parameters parameters;parameters.sizeMode=BatchProcessing::SizeMode::LongEdge;parameters.longEdge=512;
        require(batch.start(input,output,parameters),"Cannot start batch");timeout.start(15000);loop.exec();
        require(done && fatal.isEmpty() && success==1 && failed==1 && QImage(output+"/原图-result.png").size()==QSize(512,384),"Batch deployment check failed");
        done=false;require(batch.start(input,temp.filePath("取消任务"),parameters),"Cannot restart batch");batch.cancel();timeout.start(15000);loop.exec();
        require(done && success==0,"Cancelled batch wrote output");
        QImage portrait(128,160,QImage::Format_RGB888);portrait.fill(QColor(120,160,210));
        ImageProcessor::Options removal;removal.background=ImageProcessor::BackgroundMode::Remove;
        const auto transparent=ImageProcessing::processImage(portrait,removal);
        require(transparent.hasAlphaChannel() && transparent.size()==portrait.size(),"Embedded model inference failed");
        std::cout<<"PASS: deployed JPEG/PNG formats, Unicode paths, large image worker, combined pixels/alpha, unique output, mixed batch failures, cancellation and embedded model\n";
    }catch(const std::exception &error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
