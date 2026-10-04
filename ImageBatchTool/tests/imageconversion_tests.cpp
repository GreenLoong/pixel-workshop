#include "infrastructure/imageconversion.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char *message)
{
    if(!condition)throw std::runtime_error(message);
}
}

int main()
{
    try {
        // 宽度为 3 时，RGB 行步长包含填充，不能假设 width*3。
        QImage rgb(3,2,QImage::Format_RGB888);
        rgb.fill(QColor(20,80,160));rgb.setPixelColor(2,1,QColor(200,50,10));
        const cv::Mat view=ImageConversion::rgbView(rgb);
        require(view.step==static_cast<size_t>(rgb.bytesPerLine()),"RGB row stride lost");
        require(view.at<cv::Vec3b>(1,2)==cv::Vec3b(200,50,10),"RGB channel order lost");
        QImage copy=ImageConversion::copyImage(view);
        rgb.fill(Qt::black);
        require(copy.pixelColor(2,1)==QColor(200,50,10),"Output still borrows original memory");

        QImage rgba(3,2,QImage::Format_RGBA8888);rgba.fill(QColor(10,30,70,77));
        require(ImageConversion::copyImage(ImageConversion::rgbView(rgba)).pixelColor(1,1)==QColor(10,30,70,77),
                "RGBA round-trip changed alpha or colors");
        QImage gray;
        {cv::Mat temporary(2,3,CV_8UC1,cv::Scalar(76));gray=ImageConversion::copyImage(temporary);}
        require(gray.format()==QImage::Format_Grayscale8 && gray.pixelColor(2,1).red()==76,
                "Grayscale output lifetime or format wrong");
        bool rejected=false;
        try {ImageConversion::rgbView(QImage(2,2,QImage::Format_ARGB32));}
        catch(const std::exception &) {rejected=true;}
        require(rejected,"Adapter silently accepted incompatible channel layout");
        std::cout<<"PASS: padded RGB rows, channel order, alpha and independent result lifetime\n";
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
