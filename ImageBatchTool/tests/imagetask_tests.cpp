#include "application/imagetask.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
template<class Predicate> void waitFor(Predicate predicate) {
    QElapsedTimer timer;timer.start();
    while(!predicate() && timer.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(1);}
    require(predicate(),"Task timed out");
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        QImage source(16,16,QImage::Format_RGB888);source.fill(Qt::red);
        auto gate=std::make_shared<std::promise<void>>();auto released=gate->get_future().share();
        std::atomic<int> started{0},calls{0};std::atomic<bool> guiWork{false};
        ImageTask task(nullptr,[&](const QImage &image,const ImageProcessor::Options &options) {
            ++calls;if(QThread::currentThread()==app.thread())guiWork=true;
            if(options.brightness==10) {
                started=1;
                if(released.wait_for(std::chrono::seconds(5))!=std::future_status::ready)
                    throw std::runtime_error("Gate timed out");
            }
            if(options.brightness<0)throw std::runtime_error("Expected processing failure");
            QImage result=image.copy();result.fill(QColor(options.brightness,0,0));return result;
        });
        int completed=0,last=-1;QString error;
        QObject::connect(&task,&ImageTask::completed,&app,[&](const QImage &image,const QString &failure) {
            require(QThread::currentThread()==app.thread(),"Result not delivered on GUI thread");
            ++completed;error=failure;if(!image.isNull())last=image.pixelColor(0,0).red();
        });
        ImageProcessor::Options options;options.brightness=10;task.submit(source,options);
        waitFor([&]{return started.load()==1;});
        options.brightness=20;task.submit(source,options);options.brightness=30;task.submit(source,options);
        gate->set_value();waitFor([&]{return completed==1;});
        require(last==30 && calls==2 && !guiWork,"Latest request coalescing or worker thread failed");
        options.brightness=40;task.submit(source,options);task.cancel();
        options.brightness=50;task.submit(source,options);waitFor([&]{return completed==2;});
        require(last==50,"Cancelled result overwrote the latest request");
        options.brightness=-1;task.submit(source,options);waitFor([&]{return completed==3;});
        require(!error.isEmpty() && !task.isBusy(),"Exception did not become a recoverable result");
        ImageTask realTask;QImage actual;
        QObject::connect(&realTask,&ImageTask::completed,&app,[&](const QImage &image,const QString &failure){error=failure;actual=image;});
        options={};options.grayscale=true;realTask.submit(source,options);waitFor([&]{return !actual.isNull();});
        require(error.isEmpty() && actual.pixelColor(0,0).red()==76 && source.pixelColor(0,0)==QColor(Qt::red),
                "Real pipeline changed source or produced wrong pixels");
        std::atomic<bool> destroyedTaskFinished{false};
        auto *temporary=new ImageTask(nullptr,[&](const QImage &image,const auto &){destroyedTaskFinished=true;return image;});
        temporary->submit(source,{});delete temporary;waitFor([&]{return destroyedTaskFinished.load();});
        std::cout<<"PASS: worker thread, latest-only results, cancellation, failure recovery, original preservation and safe destruction\n";
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
