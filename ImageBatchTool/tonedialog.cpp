#include "tonedialog.h"
#include "dialogappearance.h"
#include "imageprocessing.h"
#include "previewlabel.h"
#include "sliderstyle.h"
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <exception>

ToneDialog::ToneDialog(const QImage &original,const ImageProcessor::Options &options,QWidget *parent)
    : QDialog(parent),initial_(options),working_(options),preview_(new PreviewLabel(this)),
      brightness_(new QSpinBox(this)),contrast_(new QDoubleSpinBox(this)),
      buttons_(new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this)),
      error_(new QLabel(this))
{
    setObjectName("ToneDialog"); setWindowTitle("颜色与光线"); resize(1080,720); setMinimumSize(800,620);
    auto *root=new QVBoxLayout(this); root->setContentsMargins(12,12,12,12); root->setSpacing(0);
    auto *content=new QWidget(this); content->setObjectName("contentPanel");
    auto *body=new QVBoxLayout(content); body->setContentsMargins(24,20,24,20); body->setSpacing(16);
    root->addWidget(content,1);
    auto *header=new QHBoxLayout;
    auto *heading=new QLabel("颜色与光线",this); heading->setObjectName("dialogHeading");
    header->addWidget(heading,1);
    auto *close=new QToolButton(this); close->setObjectName("closeButton");
    close->setIcon(QIcon(":/indicators/close.svg")); close->setFixedSize(32,32); header->addWidget(close);
    body->addLayout(header);
    auto *hint=new QLabel("拖动即时预览，点击轨道直接定位。确定应用，取消保留当前图片。",this);
    hint->setObjectName("modeHint"); body->addWidget(hint);
    auto *editor=new QHBoxLayout; editor->setSpacing(20);
    preview_->setObjectName("tonePreview"); editor->addWidget(preview_,1);
    auto *panel=new QWidget(this); auto *rows=new QVBoxLayout(panel); rows->setSpacing(18);
    rows->setContentsMargins(8,4,8,4);
    auto *scroll=new QScrollArea(this); scroll->setWidget(panel); scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame); scroll->setFixedWidth(320); editor->addWidget(scroll);
    body->addLayout(editor,1);
    const auto addRow=[&](const QString &label,const QString &name,int min,int max,int value,
                          QWidget *spin,auto changed) {
        auto *row=new QVBoxLayout;
        auto *top=new QHBoxLayout; top->addWidget(new QLabel(label,this),1); top->addWidget(spin);
        row->addLayout(top);
        auto *slider=new QSlider(Qt::Horizontal,this); slider->setObjectName(name+"Slider");
        slider->setRange(min,max); slider->setValue(value); AbsoluteSliderStyle::applyTo(slider);
        row->addWidget(slider); rows->addLayout(row);
        connect(slider,&QSlider::valueChanged,this,changed);
        return slider;
    };
    brightness_->setObjectName("brightnessSpinBox"); brightness_->setRange(-100,100);
    brightness_->setValue(options.brightness); brightness_->setFixedWidth(94);
    brightness_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    auto *bs=addRow("亮度","brightness",-100,100,options.brightness,brightness_,
        [this](int value){brightness_->setValue(value);});
    connect(brightness_,&QSpinBox::valueChanged,this,[this,bs](int value) {
        const QSignalBlocker block(bs); bs->setValue(value); working_.brightness=value; updatePreview();
    });
    contrast_->setObjectName("contrastSpinBox"); contrast_->setRange(.5,2); contrast_->setDecimals(2);
    contrast_->setValue(options.contrast); contrast_->setFixedWidth(94);
    contrast_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    auto *cs=addRow("对比度","contrast",50,200,qRound(options.contrast*100),contrast_,
        [this](int value){contrast_->setValue(value/100.0);});
    connect(contrast_,&QDoubleSpinBox::valueChanged,this,[this,cs](double value) {
        const QSignalBlocker block(cs); cs->setValue(qRound(value*100)); working_.contrast=value; updatePreview();
    });
    auto *ev=new QDoubleSpinBox(this); ev->setObjectName("exposureSpinBox"); ev->setRange(-2,2);
    ev->setDecimals(2); ev->setSingleStep(.05); ev->setValue(options.exposure); ev->setSuffix(" EV");
    ev->setFixedWidth(94); ev->setButtonSymbols(QAbstractSpinBox::NoButtons);
    auto *es=addRow("曝光","exposure",-200,200,qRound(options.exposure*100),ev,
        [ev](int value){ev->setValue(value/100.0);});
    connect(ev,&QDoubleSpinBox::valueChanged,this,[this,es](double value){
        const QSignalBlocker block(es); es->setValue(qRound(value*100)); working_.exposure=value; updatePreview();
    });
    struct Field {const char *label;const char *name;int ImageProcessor::Options::*member;};
    for(auto f : {Field{"饱和度","saturation",&ImageProcessor::Options::saturation},
        {"色温","temperature",&ImageProcessor::Options::temperature},{"色调","tint",&ImageProcessor::Options::tint},
        {"高光","highlights",&ImageProcessor::Options::highlights},{"阴影","shadows",&ImageProcessor::Options::shadows},
        {"清晰度","clarity",&ImageProcessor::Options::clarity},{"晕影","vignette",&ImageProcessor::Options::vignette}}) {
        auto *spin=new QSpinBox(this); spin->setObjectName(QString(f.name)+"SpinBox");
        spin->setRange(-100,100); spin->setValue(options.*f.member); spin->setFixedWidth(94);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        auto *slider=addRow(QString::fromUtf8(f.label),f.name,-100,100,options.*f.member,spin,
            [spin](int value){spin->setValue(value);});
        connect(spin,&QSpinBox::valueChanged,this,[this,slider,member=f.member](int value) {
            const QSignalBlocker block(slider); slider->setValue(value); working_.*member=value; updatePreview();
        });
    }
    auto *reset=new QPushButton("重置全部颜色调整",this); reset->setObjectName("resetToneButton");
    reset->setAutoDefault(false); rows->addWidget(reset); rows->addStretch();
    error_->setWordWrap(true); error_->setStyleSheet("color:#d92d20;"); error_->hide(); body->addWidget(error_);
    auto *footer=new QWidget(this); footer->setObjectName("footerPanel");
    auto *foot=new QHBoxLayout(footer); foot->setContentsMargins(24,16,24,16); foot->addWidget(buttons_); root->addWidget(footer);
    DialogAppearance::setup(this,{heading}); DialogAppearance::setupButtons(buttons_);
    connect(close,&QToolButton::clicked,this,&QDialog::reject);
    connect(buttons_,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(reset,&QPushButton::clicked,this,[this] {
        for(auto *spin:findChildren<QSpinBox*>()) {const QSignalBlocker b(spin);spin->setValue(0);}
        for(auto *spin:findChildren<QDoubleSpinBox*>()) {
            const QSignalBlocker b(spin);spin->setValue(spin==contrast_?1:0);
        }
        for(auto *slider:findChildren<QSlider*>()) {
            const QSignalBlocker b(slider);slider->setValue(slider->objectName()=="contrastSlider"?100:0);
        }
        working_.clearTone(); updatePreview();
    });
    try {
        const QSize actual=options.targetSize==cv::Size()?original.size():QSize(options.targetSize.width,options.targetSize.height);
        const QSize small=actual.scaled(1280,1280,Qt::KeepAspectRatio).boundedTo(actual);
        auto base=options; base.clearTone(); base.grayscale=false;
        base.targetSize=options.targetSize==cv::Size()?cv::Size():cv::Size(small.width(),small.height());
        const QImage source=original.width()>1280||original.height()>1280
            ?original.scaled(1280,1280,Qt::KeepAspectRatio,Qt::SmoothTransformation):original;
        basePreview_=ImageProcessing::processImage(source,base);
        if(basePreview_.width()>1280 || basePreview_.height()>1280)
            basePreview_=basePreview_.scaled(1280,1280,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        updatePreview();
    } catch(const std::exception &e) {
        error_->setText("预览失败："+QString::fromUtf8(e.what()));error_->show();
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}
ImageProcessor::Options ToneDialog::options() const {return working_;}
void ToneDialog::updatePreview()
{
    if(basePreview_.isNull())return;
    try {
        auto tone=working_; tone.rotation=0; tone.flipHorizontal=tone.flipVertical=false; tone.crop={};tone.targetSize={};
        // 基础预览包含灰度状态，扩展色彩处理之后仍保持灰度。
        preview_->setImage(QPixmap::fromImage(ImageProcessing::processImage(basePreview_,tone)));
        error_->hide();buttons_->button(QDialogButtonBox::Ok)->setEnabled(true);
    } catch(const std::exception &e) {
        error_->setText("预览失败："+QString::fromUtf8(e.what()));error_->show();
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}
