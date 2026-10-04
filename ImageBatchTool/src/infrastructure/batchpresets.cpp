#include "infrastructure/batchpresets.h"
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <stdexcept>
namespace {
QString key(const QString &name) {
    const auto trimmed=name.trimmed();
    if(trimmed.isEmpty() || trimmed.size()>64)throw std::runtime_error("预设名称需为 1～64 个字符");
    return "batch/presets/"+QString::fromLatin1(QUrl::toPercentEncoding(trimmed));
}
QJsonArray rect(cv::Rect2d value){return {value.x,value.y,value.width,value.height};}
cv::Rect2d readRect(const QJsonValue &value) {
    const auto a=value.toArray();if(a.size()!=4)throw std::runtime_error("预设区域格式无效");
    for(const auto &v:a)if(!v.isDouble() || v.toDouble()<0 || v.toDouble()>1)throw std::runtime_error("预设区域参数无效");
    const cv::Rect2d r(a[0].toDouble(),a[1].toDouble(),a[2].toDouble(),a[3].toDouble());
    if(r.x+r.width>1+1e-9 || r.y+r.height>1+1e-9)throw std::runtime_error("预设区域超出图片范围");
    return r;
}
struct IntegerField {const char *name;int ImageProcessor::Options::*member;};
const IntegerField fields[]={{"brightness",&ImageProcessor::Options::brightness},{"saturation",&ImageProcessor::Options::saturation},
    {"temperature",&ImageProcessor::Options::temperature},{"tint",&ImageProcessor::Options::tint},
    {"highlights",&ImageProcessor::Options::highlights},{"shadows",&ImageProcessor::Options::shadows},
    {"clarity",&ImageProcessor::Options::clarity},{"vignette",&ImageProcessor::Options::vignette}};
double number(const QJsonObject &o,const char *name,double low,double high) {
    const auto value=o.value(name);
    if(!value.isDouble() || value.toDouble()<low || value.toDouble()>high)throw std::runtime_error("预设参数缺失或超出范围");
    return value.toDouble();
}
void sync(QSettings &settings){settings.sync();if(settings.status()!=QSettings::NoError)throw std::runtime_error("无法保存预设，请检查配置目录权限");}
}
QStringList BatchPresets::names() {
    QSettings settings;settings.beginGroup("batch/presets");QStringList result;
    for(const auto &encoded:settings.childKeys())result<<QUrl::fromPercentEncoding(encoded.toLatin1());
    result.sort();return result;
}
void BatchPresets::save(const QString &name,const BatchProcessing::Parameters &p) {
    const auto &v=p.processing;
    if(!v.backgroundImage.empty())throw std::runtime_error("图片背景不能保存为参数预设，请先改用纯色背景或其他背景模式");
    QJsonObject o{{"version",1},{"sizeMode",static_cast<int>(p.sizeMode)},{"width",v.targetSize.width},{"height",v.targetSize.height},
        {"longEdge",p.longEdge},{"percent",p.percent},{"allowUpscale",p.allowUpscale},
        {"grayscale",v.grayscale},{"contrast",v.contrast},{"exposure",v.exposure},{"rotation",v.rotation},
        {"flipHorizontal",v.flipHorizontal},{"flipVertical",v.flipVertical},{"crop",rect(v.crop)},
        {"background",static_cast<int>(v.background)},{"segmentation",static_cast<int>(v.segmentation)},
        {"foregroundRect",rect(v.foregroundRect)},{"feather",v.feather},{"backgroundBlur",v.backgroundBlur},
        {"backgroundColor",QJsonArray{v.backgroundColor[0],v.backgroundColor[1],v.backgroundColor[2]}}};
    for(const auto &field:fields)o[field.name]=v.*field.member;
    QJsonArray strokes;
    for(const auto &stroke:v.strokes) {
        QJsonArray points;for(const auto &point:stroke.points)points.append(QJsonArray{point.x,point.y});
        strokes.append(QJsonObject{{"radius",stroke.radius},{"foreground",stroke.foreground},{"points",points}});
    }
    o["strokes"]=strokes;
    QSettings settings;settings.setValue(key(name),QJsonDocument(o).toJson(QJsonDocument::Compact));sync(settings);
}
BatchProcessing::Parameters BatchPresets::load(const QString &name) {
    QSettings settings;QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(settings.value(key(name)).toByteArray(),&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject() || doc.object()["version"].toInt()!=1)
        throw std::runtime_error("预设损坏或版本不受支持");
    const auto o=doc.object();BatchProcessing::Parameters p;auto &v=p.processing;
    p.sizeMode=static_cast<BatchProcessing::SizeMode>(static_cast<int>(number(o,"sizeMode",0,3)));
    v.targetSize=cv::Size(static_cast<int>(number(o,"width",0,20000)),static_cast<int>(number(o,"height",0,20000)));
    p.longEdge=static_cast<int>(number(o,"longEdge",1,20000));p.percent=number(o,"percent",1,800);p.allowUpscale=o["allowUpscale"].toBool();
    v.grayscale=o["grayscale"].toBool();v.contrast=number(o,"contrast",.5,2);v.exposure=number(o,"exposure",-2,2);
    for(const auto &field:fields)v.*field.member=static_cast<int>(number(o,field.name,-100,100));
    v.rotation=number(o,"rotation",-180,180);v.flipHorizontal=o["flipHorizontal"].toBool();v.flipVertical=o["flipVertical"].toBool();
    v.crop=readRect(o["crop"]);v.foregroundRect=readRect(o["foregroundRect"]);
    v.background=static_cast<ImageProcessor::BackgroundMode>(static_cast<int>(number(o,"background",0,3)));
    v.segmentation=static_cast<ImageProcessor::SegmentationMethod>(static_cast<int>(number(o,"segmentation",0,1)));
    v.feather=static_cast<int>(number(o,"feather",0,20));v.backgroundBlur=static_cast<int>(number(o,"backgroundBlur",1,50));
    const auto color=o["backgroundColor"].toArray();if(color.size()!=3)throw std::runtime_error("预设颜色无效");
    for(int i=0;i<3;++i) {if(!color[i].isDouble() || color[i].toDouble()<0 || color[i].toDouble()>255)throw std::runtime_error("预设颜色无效");v.backgroundColor[i]=color[i].toDouble();}
    for(const auto &entry:o["strokes"].toArray()) {
        const auto stroke=entry.toObject();ImageProcessor::BrushStroke s;
        s.radius=number(stroke,"radius",.001,1);s.foreground=stroke["foreground"].toBool();
        for(const auto &entry:stroke["points"].toArray()) {
            const auto point=entry.toArray();
            if(point.size()!=2 || !point[0].isDouble() || !point[1].isDouble()
                || point[0].toDouble()<0 || point[0].toDouble()>1 || point[1].toDouble()<0 || point[1].toDouble()>1)
                throw std::runtime_error("预设画笔坐标无效");
            s.points.emplace_back(point[0].toDouble(),point[1].toDouble());
        }
        v.strokes.push_back(std::move(s));
    }
    if(p.sizeMode==BatchProcessing::SizeMode::Exact && !ImageProcessor::validOutputSize(v.targetSize))throw std::runtime_error("预设固定尺寸无效");
    return p;
}
void BatchPresets::remove(const QString &name){QSettings settings;settings.remove(key(name));sync(settings);}
