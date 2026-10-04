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
        cv::Mat pattern(3,4,CV_8UC3);
        for(int y=0;y<3;++y)for(int x=0;x<4;++x)pattern.at<cv::Vec3b>(y,x)=cv::Vec3b(x*40,y*50,9);
        const auto saved=pattern.clone();
        ImageProcessor::Options geometry;
        geometry.flipHorizontal=true;
        auto flipped=ImageProcessor::process(pattern,geometry);
        require(flipped.at<cv::Vec3b>(0,0)==pattern.at<cv::Vec3b>(0,3),"Horizontal flip");
        geometry={}; geometry.rotation=90;
        auto rotated=ImageProcessor::process(pattern,geometry);
        require(rotated.size()==cv::Size(3,4)
            && rotated.at<cv::Vec3b>(0,2)==pattern.at<cv::Vec3b>(0,0),"90 degree rotation");
        geometry.rotation=180;
        require(ImageProcessor::process(pattern,geometry).at<cv::Vec3b>(0,0)
            ==pattern.at<cv::Vec3b>(2,3),"180 degree rotation");
        geometry={}; geometry.crop=cv::Rect2d(0.25,1.0/3,0.5,2.0/3);
        auto cropped=ImageProcessor::process(pattern,geometry);
        require(cropped.size()==cv::Size(2,2)
            && cropped.at<cv::Vec3b>(0,0)==pattern.at<cv::Vec3b>(1,1),"Normalized crop");
        geometry={}; geometry.rotation=45;
        require(ImageProcessor::process(pattern,geometry).size()==cv::Size(5,5),"Expanded canvas");
        require(cv::norm(pattern,saved,cv::NORM_INF)==0,"Geometry changed original");
        geometry.crop=cv::Rect2d(-0.1,0,1,1);
        bool cropRejected=false;
        try { ImageProcessor::process(pattern,geometry); }
        catch(const cv::Exception &) {cropRejected=true;}
        require(cropRejected,"Invalid crop accepted");
        std::cout<<"PASS: crop, flip, rotation, canvas expansion and input protection\n";
        const cv::Mat mid(8,8,CV_8UC3,cv::Scalar(64,64,64));
        ImageProcessor::Options color;
        color.exposure=1;
        require(ImageProcessor::process(mid,color).at<cv::Vec3b>(0,0)[0]==128,"Exposure stops");
        color={};color.saturation=-100;
        const auto desaturated=ImageProcessor::process(original,color).at<cv::Vec3b>(0,0);
        require(desaturated[0]==desaturated[1] && desaturated[1]==desaturated[2],"Desaturation");
        color={};color.temperature=100;
        const auto warm=ImageProcessor::process(mid,color).at<cv::Vec3b>(0,0);
        require(warm[0]>warm[2],"Warm color balance");
        color={};color.shadows=100;
        require(ImageProcessor::process(mid,color).at<cv::Vec3b>(0,0)[0]>64,"Shadow adjustment");
        color={};color.highlights=-100;
        require(ImageProcessor::process(original,color).at<cv::Vec3b>(0,0)[0]<255,"Highlight adjustment");
        color={};color.vignette=100;
        auto vignette=ImageProcessor::process(mid,color);
        require(vignette.at<cv::Vec3b>(0,0)[0]<vignette.at<cv::Vec3b>(4,4)[0],"Vignette falloff");
        color={};color.tint=10;color.clarity=30;color.saturation=20;color.exposure=.5;color.grayscale=true;
        require(ImageProcessor::process(mid,color).channels()==1,"Combined color and grayscale");
        std::cout<<"PASS: exposure, saturation, color balance, selective tones and vignette\n";
        cv::Mat rgba(12,16,CV_8UC4,cv::Scalar(255,0,0,77));
        ImageProcessor::Options alphaOptions;alphaOptions.grayscale=true;alphaOptions.brightness=10;
        alphaOptions.targetSize=cv::Size(8,6);alphaOptions.rotation=90;
        auto alphaResult=ImageProcessor::process(rgba,alphaOptions);
        require(alphaResult.type()==CV_8UC4 && alphaResult.at<cv::Vec4b>(2,2)==cv::Vec4b(86,86,86,77),"Alpha lost during processing");
        cv::Mat subject(80,80,CV_8UC3,cv::Scalar(20,35,210));
        for(int y=20;y<60;++y)for(int x=25;x<55;++x)subject.at<cv::Vec3b>(y,x)=cv::Vec3b(220,70,40);
        ImageProcessor::Options background;background.background=ImageProcessor::BackgroundMode::Remove;background.feather=0;
        background.segmentation=ImageProcessor::SegmentationMethod::Region; // 几何色块验证通用分割。
        auto removed=ImageProcessor::process(subject,background);
        require(removed.channels()==4 && removed.at<cv::Vec4b>(0,0)[3]==0
            && removed.at<cv::Vec4b>(40,40)[3]==255,"Foreground segmentation failed");
        background.background=ImageProcessor::BackgroundMode::Replace;background.backgroundColor=cv::Scalar(0,255,0);
        auto replaced=ImageProcessor::process(subject,background);
        require(replaced.at<cv::Vec3b>(0,0)==cv::Vec3b(0,255,0)
            && replaced.at<cv::Vec3b>(40,40)==subject.at<cv::Vec3b>(40,40),"Background replacement failed");
        background.background=ImageProcessor::BackgroundMode::Blur;
        require(!ImageProcessor::process(subject,background).empty(),"Background blur failed");
        background.background=ImageProcessor::BackgroundMode::Remove;
        ImageProcessor::BrushStroke stroke;stroke.foreground=false;stroke.radius=.07;stroke.points={{.5,.5}};
        background.strokes.push_back(stroke);
        require(ImageProcessor::process(subject,background).at<cv::Vec4b>(40,40)[3]==0,"Manual mask correction failed");
        require(subject.at<cv::Vec3b>(0,0)==cv::Vec3b(20,35,210),"Background processing changed input");
        std::cout<<"PASS: alpha preservation, background modes and brush mask correction\n";
        background.strokes.clear();background.foregroundRect={0,0,1,1};
        const auto fullRegion=ImageProcessor::process(subject,background);
        cv::Mat fullAlpha;cv::extractChannel(fullRegion,fullAlpha,3);
        require(cv::countNonZero(fullAlpha==255)==subject.total(),"Full selection should preserve the image without background samples");
        require(ImageProcessor::regionSamples(subject.size(),background).background==0,"Full selection sample state mismatch");
        stroke.foreground=false;stroke.radius=.1;stroke.points={{.02,.02}};
        background.strokes={stroke};
        require(ImageProcessor::regionSamples(subject.size(),background).canSegment(),"Background brush did not restore valid samples");
        const auto recovered=ImageProcessor::process(subject,background);
        require(recovered.at<cv::Vec4b>(0,0)[3]==0 && recovered.at<cv::Vec4b>(40,40)[3]==255,"Automatic segmentation did not recover after adding background samples");
        stroke.radius=.5;stroke.points={{0,0},{1,0},{1,.5},{0,.5},{0,1},{1,1}};
        background.strokes={stroke};
        cv::extractChannel(ImageProcessor::process(subject,background),fullAlpha,3);
        require(cv::countNonZero(fullAlpha)==0,"All-background brush should produce a transparent mask without throwing");
        background.foregroundRect={.1,.05,.8,.9};stroke.foreground=true;background.strokes={stroke};
        cv::extractChannel(ImageProcessor::process(subject,background),fullAlpha,3);
        require(cv::countNonZero(fullAlpha==255)==subject.total(),"All-foreground brush should preserve the image");
        background.strokes.clear();background.foregroundRect={79.0/80,79.0/80,1.0/80,1.0/80};
        const auto tinyRegion=ImageProcessor::process(subject,background);
        cv::extractChannel(tinyRegion,fullAlpha,3);
        require(cv::countNonZero(fullAlpha)==1 && fullAlpha.at<uchar>(79,79)==255,"Tiny edge selection should use the actual selected pixel");
        background.foregroundRect={.1,0,.9000000000000001,1};
        require(!ImageProcessor::process(subject,background).empty(),"Floating-point boundary rejected valid region");
        background.foregroundRect={0,0,1,1};
        cv::Mat tiny(2,2,CV_8UC3,cv::Scalar(10,20,30));
        require(ImageProcessor::process(tiny,background).at<cv::Vec4b>(1,1)[3]==255,"Tiny image cannot use manual mask fallback");
        std::cout<<"PASS: full/tiny selections, all-class strokes, floating bounds and automatic segmentation recovery\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
