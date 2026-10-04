#include "domain/batchoptions.h"
#include "application/batchjob.h"
#include "infrastructure/batchpresets.h"
#include "presentation/widgets/batchparameters.h"
#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QDir>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <iostream>
#include <stdexcept>
using namespace BatchProcessing;
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void run(const QString &input,const QString &output,Parameters p,int expectedSuccess,int expectedFailure=0) {
    BatchJob job;QEventLoop loop;QTimer timeout;bool finished=false;
    QObject::connect(&job,&BatchJob::finished,&loop,[&](int,int success,int failure,bool,const QString &error) {
        require(error.isEmpty() && success==expectedSuccess && failure==expectedFailure,"Batch result wrong");finished=true;loop.quit();
    });
    QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
    require(job.start(input,output,p),"Cannot start parameterized batch");
    p.sizeMode=SizeMode::Exact;p.processing.targetSize=cv::Size(1,1); // 已启动任务不能观察调用者后续修改。
    timeout.setSingleShot(true);timeout.start(5000);loop.exec();require(finished,"Batch timed out");
}
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);app.setOrganizationName("PixelWorkshopTests");app.setApplicationName("BatchParameters");
    QTemporaryDir temp(QDir::currentPath()+"/batch-parameters-XXXXXX");
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    try {
        require(temp.isValid(),"Temp failed");Parameters p;p.sizeMode=SizeMode::LongEdge;p.longEdge=256;p.processing.grayscale=true;p.processing.brightness=20;
        require(p.forImage({512,320}).targetSize==cv::Size(256,160),"Long edge did not preserve aspect");
        require(p.forImage({80,40}).targetSize==cv::Size(80,40),"Small image enlarged by default");
        p.allowUpscale=true;require(p.forImage({80,40}).targetSize==cv::Size(256,128),"Explicit upscaling failed");p.allowUpscale=false;
        p.processing.rotation=90;p.processing.crop=cv::Rect2d(0,0,1,.5);
        require(p.forImage({512,320}).targetSize==cv::Size(256,205),"Resize ignored post-geometry dimensions");
        p.processing.rotation=0;p.processing.crop={};
        const auto input=temp.filePath("输入");require(QDir().mkpath(input),"Input directory failed");
        QImage image(512,320,QImage::Format_RGB888);image.fill(Qt::red);require(image.save(input+"/大.png"),"Fixture failed");
        image=QImage(80,40,QImage::Format_RGB888);image.fill(Qt::red);require(image.save(input+"/小.png"),"Fixture failed");
        const auto longOutput=temp.filePath("长边结果");run(input,longOutput,p,2);
        require(QImage(longOutput+"/大-result.png").size()==QSize(256,160) && QImage(longOutput+"/小-result.png").size()==QSize(80,40),"Per-file long-edge sizing or snapshot failed");
        p.sizeMode=SizeMode::Percent;p.percent=50;const auto percentOutput=temp.filePath("百分比结果");run(input,percentOutput,p,2);
        require(QImage(percentOutput+"/大-result.png").size()==QSize(256,160) && QImage(percentOutput+"/小-result.png").size()==QSize(40,20)
            && QImage(percentOutput+"/小-result.png").pixelColor(0,0).red()==96,"Per-file percent or combined color failed");
        p.processing.exposure=.5;p.processing.rotation=90;p.processing.crop=cv::Rect2d(.1,.1,.8,.8);
        p.processing.background=ImageProcessor::BackgroundMode::Replace;p.processing.backgroundColor=cv::Scalar(20,80,120);
        p.processing.strokes={ImageProcessor::BrushStroke{{{.2,.3},{.25,.35}},.03,false}};
        const QString name="简历/灰度";BatchPresets::save(name,p);const auto loaded=BatchPresets::load(name);
        require(loaded.processing==p.processing && loaded.sizeMode==p.sizeMode && loaded.percent==50,"Preset round trip lost parameters");
        require(BatchPresets::names().contains(name),"Unicode slash name lost");
        BatchPresets::remove(name);require(!BatchPresets::names().contains(name),"Preset removal failed");
        QSettings().setValue("batch/presets/broken",QByteArray("{broken"));bool rejected=false;
        try{BatchPresets::load("broken");}catch(const std::exception &){rejected=true;}
        require(rejected,"Corrupt preset accepted");BatchPresets::remove("broken");
        p.processing.backgroundImage=cv::Mat(4,4,CV_8UC3);rejected=false;
        try{BatchPresets::save("unsupported",p);}catch(const std::exception &){rejected=true;}
        require(rejected && !BatchPresets::names().contains("unsupported"),"Unsupported image background silently lost");
        p.processing.backgroundImage.release();p.sizeMode=SizeMode::Exact;p.processing.targetSize={10000,10000};run(input,temp.filePath("超限"),p,0,2);
        ImageProcessor::Options home;home.targetSize={300,200};home.exposure=.5;home.grayscale=true;
        BatchParameters widget(home);widget.show();QApplication::processEvents();
        require(widget.parameters().processing.targetSize==cv::Size(300,200) && widget.parameters().processing.exposure==.5,"Initial home parameters lost");
        auto *presets=widget.findChild<QComboBox *>("batchPreset");presets->setCurrentIndex(presets->findData("long1200"));
        widget.findChild<QSpinBox *>("batchLongEdge")->setValue(275);widget.findChild<QCheckBox *>("batchGrayscale")->setChecked(true);
        require(widget.parameters().longEdge==275 && widget.parameters().sizeMode==SizeMode::LongEdge && widget.parameters().processing.grayscale
            && widget.parameters().processing.exposure==0,"Independent parameters or preset reset failed");
        widget.setEnabled(false);require(!widget.findChild<QSpinBox *>("batchLongEdge")->isEnabled(),"Running task can edit batch settings");
        std::cout<<"PASS: per-file aspect sizing, no default upscaling, percentage, immutable snapshot, preset persistence/validation and independent UI\n";
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
