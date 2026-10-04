#include "imageprocessor.h"

#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try {
        const cv::Mat original(4,8,CV_8UC3,cv::Scalar(255,0,0)); // RGB 纯红色。
        // 验证灰度、尺寸、亮度、对比度的全部 16 种开关组合。
        for (int mask=0; mask<16; ++mask) {
            ImageProcessor::Options options;
            options.grayscale = mask & 1;
            if (mask & 2) options.targetSize = cv::Size(4,2);
            options.brightness = (mask & 4) ? 10 : 0;
            options.contrast = (mask & 8) ? 1.5 : 1.0;
            const cv::Mat result = ImageProcessor::process(original,options);
            require(result.size()==((mask & 2) ? cv::Size(4,2) : original.size()),"Output size");
            require(result.channels()==(options.grayscale ? 1 : 3),"Output channels");
            if (options.grayscale) {
                const int expected = (mask & 8) ? ((mask & 4) ? 124 : 114)
                                               : ((mask & 4) ? 86 : 76);
                require(result.at<uchar>(0,0)==expected,"Combined grayscale/tone pixels");
            } else {
                const cv::Vec3b pixel = result.at<cv::Vec3b>(0,0);
                require(pixel[0]==255 && pixel[1]==options.brightness
                        && pixel[2]==options.brightness,"RGB order and saturation");
            }
        }
        auto neutral = ImageProcessor::process(original,{});
        require(cv::norm(neutral,original,cv::NORM_INF)==0,"Identity changed pixels");
        neutral.setTo(cv::Scalar(0,0,0));
        require(original.at<cv::Vec3b>(0,0)[0]==255,"Output aliases the original");
        require(ImageProcessor::adjustTone(cv::Mat(1,1,CV_8UC1,cv::Scalar(0)),-100,2)
                .at<uchar>(0,0)==0,"Negative values wrapped");
        require(ImageProcessor::adjustTone(cv::Mat(1,1,CV_8UC1,cv::Scalar(255)),100,2)
                .at<uchar>(0,0)==255,"High values wrapped");
        for (int caseId=0; caseId<5; ++caseId) {
            ImageProcessor::Options invalid;
            if (caseId==0) invalid.brightness=101;
            if (caseId==1) invalid.contrast=std::numeric_limits<double>::quiet_NaN();
            if (caseId==2) invalid.targetSize=cv::Size(-1,2);
            if (caseId==3) invalid.targetSize=cv::Size(10000,10000);
            if (caseId==4) invalid.contrast=2.01;
            bool rejected=false;
            try { ImageProcessor::process(original,invalid); }
            catch (const cv::Exception &) { rejected=true; }
            require(rejected,"Invalid parameters were accepted");
        }
        require(original.at<cv::Vec3b>(0,0)==cv::Vec3b(255,0,0),"Input was modified");
        std::cout << "PASS: 16 processing combinations, identity, saturation, validation, original protection\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
