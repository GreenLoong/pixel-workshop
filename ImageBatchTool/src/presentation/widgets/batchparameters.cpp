#include "presentation/widgets/batchparameters.h"
#include "infrastructure/batchpresets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QLineEdit>
#include <QSignalBlocker>
#include <exception>
using namespace BatchProcessing;
BatchParameters::BatchParameters(const ImageProcessor::Options &current,QWidget *parent):QWidget(parent)
{
    setObjectName("batchParameters");
    auto *root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(10);
    auto *presets=new QHBoxLayout;root->addLayout(presets);presets->addWidget(new QLabel("参数预设",this));
    preset_=new QComboBox(this);preset_->setObjectName("batchPreset");preset_->setPlaceholderText("当前参数（可直接修改）");presets->addWidget(preset_,1);
    auto *save=new QPushButton("保存预设",this);save->setObjectName("saveBatchPreset");save->setAutoDefault(false);presets->addWidget(save);
    deletePreset_=new QPushButton("删除预设",this);deletePreset_->setAutoDefault(false);presets->addWidget(deletePreset_);
    auto *adopt=new QPushButton("使用主页参数",this);adopt->setAutoDefault(false);presets->addWidget(adopt);
    auto *sizeRow=new QHBoxLayout;root->addLayout(sizeRow);sizeRow->addWidget(new QLabel("输出尺寸",this));
    sizeMode_=new QComboBox(this);sizeMode_->setObjectName("batchSizeMode");
    sizeMode_->addItems({"保持尺寸","固定宽高","限制长边","按百分比"});sizeRow->addWidget(sizeMode_);
    sizes_=new QStackedWidget(this);sizeRow->addWidget(sizes_,1);
    sizes_->addWidget(new QLabel("保留每张图片的几何处理尺寸",this));
    auto *exact=new QWidget(this);auto *exactRow=new QHBoxLayout(exact);exactRow->setContentsMargins(0,0,0,0);
    const auto spin=[this](const char *name,int min,int max) {
        auto *s=new QSpinBox(this);s->setObjectName(name);s->setRange(min,max);s->setButtonSymbols(QAbstractSpinBox::NoButtons);return s;
    };
    width_=spin("batchWidth",1,20000);height_=spin("batchHeight",1,20000);
    width_->setFixedWidth(110);height_->setFixedWidth(110);
    exactRow->addWidget(width_);exactRow->addWidget(new QLabel("×",this));exactRow->addWidget(height_);exactRow->addWidget(new QLabel("px（固定宽高）",this));exactRow->addStretch();sizes_->addWidget(exact);
    auto *longest=new QWidget(this);auto *longRow=new QHBoxLayout(longest);longRow->setContentsMargins(0,0,0,0);
    longEdge_=spin("batchLongEdge",1,20000);longEdge_->setSuffix(" px");longRow->addWidget(longEdge_);
    upscale_=new QCheckBox("允许放大小图",this);upscale_->setObjectName("batchUpscale");longRow->addWidget(upscale_);sizes_->addWidget(longest);
    percent_=new QDoubleSpinBox(this);percent_->setObjectName("batchPercent");percent_->setRange(1,800);percent_->setDecimals(1);percent_->setSuffix(" %");sizes_->addWidget(percent_);
    auto *tone=new QHBoxLayout;root->addLayout(tone);
    gray_=new QCheckBox("灰度化",this);gray_->setObjectName("batchGrayscale");tone->addWidget(gray_);
    brightness_=spin("batchBrightness",-100,100);tone->addWidget(new QLabel("亮度",this));tone->addWidget(brightness_);
    contrast_=new QDoubleSpinBox(this);contrast_->setObjectName("batchContrast");contrast_->setRange(.5,2);contrast_->setDecimals(2);contrast_->setSingleStep(.05);
    contrast_->setButtonSymbols(QAbstractSpinBox::NoButtons);tone->addWidget(new QLabel("对比度",this));tone->addWidget(contrast_);
    background_=new QComboBox(this);background_->setObjectName("batchBackground");background_->addItems({"保留背景","模糊背景","移除背景（透明）","替换为纯色"});tone->addWidget(background_,1);
    auto *backgroundRow=new QHBoxLayout;root->addLayout(backgroundRow);
    color_=new QPushButton("背景颜色",this);color_->setAutoDefault(false);backgroundRow->addWidget(color_);
    feather_=spin("batchFeather",0,20);backgroundRow->addWidget(new QLabel("边缘柔和度",this));backgroundRow->addWidget(feather_);
    blur_=spin("batchBlur",1,50);backgroundRow->addWidget(new QLabel("模糊强度",this));backgroundRow->addWidget(blur_);backgroundRow->addStretch();
    extra_=new QLabel(this);extra_->setWordWrap(true);root->addWidget(extra_);
    connect(sizeMode_,&QComboBox::currentIndexChanged,this,[this](int i){sizes_->setCurrentIndex(i);changed();});
    connect(background_,&QComboBox::currentIndexChanged,this,[this]{if(!loading_)draft_.processing.backgroundImage.release();changed();});
    for(auto *s:{width_,height_,longEdge_,brightness_,blur_,feather_})connect(s,&QSpinBox::valueChanged,this,&BatchParameters::changed);
    for(auto *s:{percent_,contrast_})connect(s,&QDoubleSpinBox::valueChanged,this,&BatchParameters::changed);
    for(auto *s:{gray_,upscale_})connect(s,&QCheckBox::toggled,this,&BatchParameters::changed);
    connect(color_,&QPushButton::clicked,this,[this] {
        const auto &v=draft_.processing.backgroundColor;
        const auto selected=QColorDialog::getColor(QColor(v[0],v[1],v[2]),this,"批量背景颜色",QColorDialog::DontUseNativeDialog);
        if(selected.isValid()){draft_.processing.backgroundColor=cv::Scalar(selected.red(),selected.green(),selected.blue());draft_.processing.backgroundImage.release();changed();}
    });
    connect(adopt,&QPushButton::clicked,this,[this,current]{load(Parameters::fromOptions(current));});
    connect(preset_,&QComboBox::currentIndexChanged,this,[this](int index) {
        if(loading_ || index<0)return;
        try {
            Parameters value;const auto id=preset_->currentData().toString();
            if(id=="original"){}
            else if(id=="long1200"){value.sizeMode=SizeMode::LongEdge;value.longEdge=1200;}
            else if(id=="gray")value.processing.grayscale=true;
            else if(id=="remove")value.processing.background=ImageProcessor::BackgroundMode::Remove;
            else value=BatchPresets::load(id.mid(5));
            load(value);loading_=true;preset_->setCurrentIndex(index);loading_=false;
            deletePreset_->setEnabled(index>=4);
        }catch(const std::exception &e){QMessageBox::warning(this,"无法读取预设",QString::fromUtf8(e.what()));}
    });
    connect(save,&QPushButton::clicked,this,[this] {
        bool accepted=false;const auto name=QInputDialog::getText(this,"保存批量预设","预设名称（同名更新）",QLineEdit::Normal,{},&accepted);
        if(!accepted)return;
        try{BatchPresets::save(name,parameters());reloadPresets(name.trimmed());}
        catch(const std::exception &e){QMessageBox::warning(this,"保存预设失败",QString::fromUtf8(e.what()));}
    });
    connect(deletePreset_,&QPushButton::clicked,this,[this] {
        if(preset_->currentIndex()<4)return;
        try{BatchPresets::remove(preset_->currentData().toString().mid(5));reloadPresets();}
        catch(const std::exception &e){QMessageBox::warning(this,"删除预设失败",QString::fromUtf8(e.what()));}
    });
    reloadPresets();load(Parameters::fromOptions(current));
}
Parameters BatchParameters::parameters() const {
    auto result=draft_;auto &p=result.processing;
    result.sizeMode=static_cast<SizeMode>(sizeMode_->currentIndex());
    p.targetSize=result.sizeMode==SizeMode::Exact?cv::Size(width_->value(),height_->value()):cv::Size();
    result.longEdge=longEdge_->value();result.percent=percent_->value();result.allowUpscale=upscale_->isChecked();
    p.grayscale=gray_->isChecked();p.brightness=brightness_->value();p.contrast=contrast_->value();
    p.background=static_cast<ImageProcessor::BackgroundMode>(background_->currentIndex());p.feather=feather_->value();p.backgroundBlur=blur_->value();
    return result;
}
void BatchParameters::load(const Parameters &p) {
    loading_=true;draft_=p;
    sizeMode_->setCurrentIndex(static_cast<int>(p.sizeMode));
    width_->setValue(p.processing.targetSize.width>0?p.processing.targetSize.width:800);
    height_->setValue(p.processing.targetSize.height>0?p.processing.targetSize.height:600);
    longEdge_->setValue(p.longEdge);percent_->setValue(p.percent);upscale_->setChecked(p.allowUpscale);
    gray_->setChecked(p.processing.grayscale);brightness_->setValue(p.processing.brightness);contrast_->setValue(p.processing.contrast);
    background_->setCurrentIndex(static_cast<int>(p.processing.background));feather_->setValue(p.processing.feather);blur_->setValue(p.processing.backgroundBlur);
    loading_=false;changed();
}
void BatchParameters::changed() {
    color_->setEnabled(background_->currentIndex()==3);blur_->setEnabled(background_->currentIndex()==1);
    feather_->setEnabled(background_->currentIndex()!=0);
    QStringList additional;const auto &p=draft_.processing;
    if(p.hasColorAdjustments())additional<<"其他颜色调整";
    if(p.rotation || p.flipHorizontal || p.flipVertical || p.crop!=cv::Rect2d())additional<<"裁剪／旋转／翻转";
    if(!p.strokes.empty())additional<<"画笔修正";
    if(!p.backgroundImage.empty())additional<<"图片背景（本次有效）";
    extra_->setText(additional.isEmpty()?"长边与百分比按每张图片的尺寸计算，保持比例。":
        "同时沿用："+additional.join(" · ")+"。裁剪和画笔按相对位置应用，请先少量试运行。");
    if(!loading_){const QSignalBlocker block(preset_);preset_->setCurrentIndex(-1);deletePreset_->setEnabled(false);}
}
void BatchParameters::reloadPresets(const QString &selected) {
    const QSignalBlocker block(preset_);preset_->clear();
    preset_->addItem("保持原图／转换 PNG","original");preset_->addItem("长边限制为 1200 px","long1200");
    preset_->addItem("灰度转换","gray");preset_->addItem("人像移除背景","remove");
    for(const auto &name:BatchPresets::names())preset_->addItem(name,"user/"+name);
    preset_->setCurrentIndex(selected.isEmpty()?-1:preset_->findData("user/"+selected));deletePreset_->setEnabled(!selected.isEmpty());
}
