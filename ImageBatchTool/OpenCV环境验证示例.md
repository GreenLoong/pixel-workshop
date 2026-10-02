# OpenCV 环境验证示例

- 整理日期：2026-10-02
- 用途：记录已通过的 OpenCV 灰度转换、PNG 保存和重新读取验证。
- 使用方式：需要排查环境时，可临时替换 ImageBatchTool/main.cpp，验证后恢复正常入口。
- 输出位置：运行工作目录下的 temp/environment-check.png。
- 注意：重复运行会覆盖同名测试图片。

```cpp
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QDir>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    //创建临时文件夹
    if (!QDir().mkpath("temp")) {
        qCritical() << "无法创建 temp 文件夹";
        return 1;
    }

    const std::string outputPath = "temp/environment-check.png";

    qInfo() << "保存位置:" << QDir::current().absoluteFilePath("temp/environment-check.png");


    //先创建一张图片，将图片转化为灰度图片，用来验证opencv正常运行
    try
    {
        //创建一个32*32，8位比特位（整数）3通道，初始化值为0,0,255的图片（红色）
        cv::Mat soure(32,32,CV_8UC3,cv::Scalar(255,0,0));

        //创建灰度副本
        cv::Mat gray;
        //将soure按照rgb转化为灰度图片保存到gray
        cv::cvtColor(soure,gray,cv::COLOR_RGB2GRAY);

        //保存图片到本地
        if(!cv::imwrite(outputPath,gray))
        {
            qCritical() << "图片保存失败";
            return 2;
        }

        //重新读取保存的图片
        cv::Mat loaded = cv::imread(outputPath,cv::IMREAD_GRAYSCALE);

        if(loaded.empty())
        {
            qCritical() << "图片读取失败";
            return 3;
        }

        //检查读取到的图片和原图片是不是同一张图片
        if( loaded.size() != gray.size() ||                 //大小
            loaded.type() != gray.type() ||                 //类型
            cv:: norm(loaded, gray, cv::NORM_INF) != 0 ||   //全部像素绝对值
            loaded.at<uchar>(0,0) != 76)                    //左上角灰度值是否为红色的灰度值 灰度值=R x 0.299 + G x 0.587 + B x 0.114 ,纯红的灰度约为76
        {
            qCritical() << "图片内容验证失败";
            return 4;
        }

        qInfo() << "OpenCV 版本:" << CV_VERSION;
        qInfo() << "OpenCV 成功运行";


    }
    catch(const cv::Exception& error)
    {
        qCritical() << "OpenCV 运行失败:" << QString::fromUtf8(error.what());
        return 4;
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}

```