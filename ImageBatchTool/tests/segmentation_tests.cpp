#include "application/imageprocessing.h"
#include <QCoreApplication>
#include <QImage>
#include <QDir>
#include <future>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    try {
        QImage blank(320,240,QImage::Format_RGB888);blank.fill(Qt::white);
        ImageProcessor::Options options;options.background=ImageProcessor::BackgroundMode::Remove;options.feather=0;
        QImage mask=ImageProcessing::processImage(blank,options);
        require(mask.size()==blank.size() && mask.hasAlphaChannel(),"Model output dimensions/format invalid");
        ImageProcessor::BrushStroke stroke;stroke.foreground=true;stroke.radius=.1;stroke.points={{.5,.5}};
        options.strokes.push_back(stroke);
        require(ImageProcessing::processImage(blank,options).pixelColor(160,120).alpha()==255,"Foreground brush ignored by model");
        options.strokes.back().foreground=false;
        require(ImageProcessing::processImage(blank,options).pixelColor(160,120).alpha()==0,"Background brush ignored by model");
        options.strokes.clear();
        auto a=std::async(std::launch::async,[&]{return ImageProcessing::processImage(blank,options);});
        auto b=std::async(std::launch::async,[&]{return ImageProcessing::processImage(blank,options);});
        require(a.get()==b.get(),"Parallel model sessions are inconsistent");
        std::cout<<"PASS: embedded model checksum, inference, brush override and independent concurrent sessions\n";
        if(argc>1) {
            const QImage portrait(QString::fromLocal8Bit(argv[1]));require(!portrait.isNull(),"Portrait fixture unreadable");
            const QImage before=portrait.copy();
            options.feather=2;
            const QImage result=ImageProcessing::processImage(portrait,options);
            require(result.save(QDir::currentPath()+"/human-removal-check.png"),"Could not save portrait result");
            double clothingAlpha=0;int count=0;
            for(int y=portrait.height()*.8;y<portrait.height()*.98;++y)
                for(int x=portrait.width()*.1;x<portrait.width()*.9;++x){clothingAlpha+=result.pixelColor(x,y).alpha();++count;}
            clothingAlpha/=count;
            std::cout<<"Clothing mean alpha: "<<clothingAlpha<<"\n";
            auto regionOptions=options;regionOptions.segmentation=ImageProcessor::SegmentationMethod::Region;
            const auto region=ImageProcessing::processImage(portrait,regionOptions);
            double oldClothingAlpha=0;
            for(int y=portrait.height()*.8;y<portrait.height()*.98;++y)
                for(int x=portrait.width()*.1;x<portrait.width()*.9;++x)oldClothingAlpha+=region.pixelColor(x,y).alpha();
            std::cout<<"Region method clothing mean alpha: "<<oldClothingAlpha/count<<"\n";
            require(clothingAlpha>240,"Portrait clothing was removed");
            require(result.pixelColor(portrait.width()/2,portrait.height()/2).alpha()>240,"Portrait face was removed");
            require(result.pixelColor(2,2).alpha()<10,"Portrait white background retained");
            require(portrait==before,"Model modified original portrait");
            options.background=ImageProcessor::BackgroundMode::Replace;options.backgroundColor=cv::Scalar(20,160,220);
            const auto replaced=ImageProcessing::processImage(portrait,options);
            require(replaced.pixelColor(2,2)==QColor(20,160,220),"Portrait replacement wrong");
            options.background=ImageProcessor::BackgroundMode::Blur;
            require(!ImageProcessing::processImage(portrait,options).isNull(),"Portrait blur failed");
            std::cout<<"PASS: portrait face/clothing preservation, background removal/replacement/blur and original protection\n";
        }
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
