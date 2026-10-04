#include "presentation/dialogs/geometrydialog.h"
#include "presentation/widgets/dialogappearance.h"
#include "application/imageprocessing.h"
#include "presentation/widgets/previewlabel.h"
#include "presentation/widgets/selectionitem.h"
#include "presentation/widgets/sliderstyle.h"
#include "presentation/widgets/previewimage.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QIcon>
#include <exception>
#include <cmath>

GeometryDialog::GeometryDialog(const QImage &original, const ImageProcessor::Options &options, QWidget *parent,PreviewLabel *sharedPreview)
    : QDialog(parent), previewSource_(PreviewImage::thumbnail(original)),
      originalSize_(original.size()), initial_(options), working_(options),
      preview_(sharedPreview?sharedPreview:new PreviewLabel(this)), selection_(new SelectionItem),
      angle_(new QDoubleSpinBox(this)), sizeLabel_(new QLabel(this)),
      buttons_(new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this)),
      ratio_(new QComboBox(this)),sharedPreview_(sharedPreview!=nullptr)
{
    setObjectName("GeometryDialog");
    resize(960,760); setMinimumSize(720,620);
    auto *root = new QVBoxLayout(this); root->setContentsMargins(12,12,12,12); root->setSpacing(0);
    auto *content = new QWidget(this); content->setObjectName("contentPanel");
    auto *body = new QVBoxLayout(content); body->setContentsMargins(24,20,24,20); body->setSpacing(14);
    if(sharedPreview_)body->setAlignment(Qt::AlignTop);
    root->addWidget(content,1);
    auto *heading = new QLabel("裁剪与旋转",this); heading->setObjectName("dialogHeading");
    auto *header = new QHBoxLayout; header->addWidget(heading,1);
    auto *close = new QToolButton(this); close->setObjectName("closeButton");
    close->setIcon(QIcon(":/indicators/close.svg")); close->setFixedSize(32,32);
    header->addWidget(close); body->addLayout(header);
    auto *hint = new QLabel("拖动边缘调整裁剪框，拖动框内移动。修改裁剪后采用裁剪尺寸，可继续使用“调整大小”。",this);
    hint->setObjectName("modeHint"); hint->setWordWrap(true); body->addWidget(hint);
    auto *tools = new QHBoxLayout;
    if(sharedPreview_)tools->setDirection(QBoxLayout::TopToBottom);
    const auto button = [&](const QString &text, const QString &name, auto callback) {
        auto *b = new QPushButton(text,this); b->setObjectName(name); b->setAutoDefault(false);
        tools->addWidget(b); connect(b,&QPushButton::clicked,this,callback);
    };
    button("左转 90°","rotateLeftButton",[this] { double a=angle_->value()-90; if(a < -180)a+=360; angle_->setValue(a); });
    button("右转 90°","rotateRightButton",[this] { double a=angle_->value()+90; if(a > 180)a-=360; angle_->setValue(a); });
    button("水平翻转","flipHorizontalButton",[this] { working_.flipHorizontal=!working_.flipHorizontal; updateImage(); });
    button("垂直翻转","flipVerticalButton",[this] { working_.flipVertical=!working_.flipVertical; updateImage(); });
    button("重置","resetGeometryButton",[this] { working_.flipHorizontal=working_.flipVertical=false; angle_->setValue(0); updateImage(); });
    body->addLayout(tools);
    if(!sharedPreview_) {
        preview_->setObjectName("geometryPreview"); preview_->setCornerRadius(8);
        preview_->setStyleSheet("QGraphicsView {background:#f2f3f5; border:1px solid #e3e6ec; border-radius:8px;}");
        body->addWidget(preview_,1);
    }
    selection_->setParent(this);
    preview_->scene()->addItem(selection_);
    auto *controls = new QHBoxLayout;
    if(sharedPreview_)controls->setDirection(QBoxLayout::TopToBottom);
    sizeLabel_->setWordWrap(true);
    controls->addWidget(new QLabel("比例",this));
    ratio_->setObjectName("cropRatio");
    for(auto pair : {std::pair<const char*,double>{"自由",0},{"原图比例",static_cast<double>(original.width())/original.height()},
        {"1:1",1},{"4:3",4.0/3},{"16:9",16.0/9},{"3:4",3.0/4},{"9:16",9.0/16}})
        ratio_->addItem(QString::fromUtf8(pair.first),pair.second);
    controls->addWidget(ratio_); controls->addWidget(new QLabel("角度",this));
    auto *slider = new QSlider(Qt::Horizontal,this); slider->setObjectName("rotationSlider");
    slider->setRange(-1800,1800); slider->setValue(qRound(options.rotation*10)); AbsoluteSliderStyle::applyTo(slider);
    controls->addWidget(slider,sharedPreview_?0:1);
    angle_->setObjectName("rotationSpinBox"); angle_->setRange(-180,180); angle_->setDecimals(1);
    angle_->setValue(options.rotation); angle_->setSuffix("°"); angle_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    controls->addWidget(angle_); body->addLayout(controls); body->addWidget(sizeLabel_);
    body->removeItem(tools);body->addLayout(tools);
    auto *footer = new QWidget(this); footer->setObjectName("footerPanel");
    auto *foot = new QHBoxLayout(footer); foot->setContentsMargins(24,16,24,16); foot->addWidget(buttons_); root->addWidget(footer);
    DialogAppearance::setup(this,{heading}); DialogAppearance::setupButtons(buttons_);
    connect(close,&QToolButton::clicked,this,&QDialog::reject);
    connect(buttons_,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(slider,&QSlider::valueChanged,this,[this](int value) {angle_->setValue(value/10.0);});
    connect(angle_,&QDoubleSpinBox::valueChanged,this,[this,slider](double value) {
        const QSignalBlocker blocker(slider); slider->setValue(qRound(value*10)); working_.rotation=value; updateImage();
    });
    connect(ratio_,&QComboBox::currentIndexChanged,this,[this] { selection_->setRatio(ratio_->currentData().toDouble()); });
    connect(selection_,&SelectionItem::selectionChanged,this,[this] {updateSize();});
    connect(&task_,&ImageTask::completed,this,[this](const QImage &image,const QString &error) {
        if(!error.isEmpty()) {sizeLabel_->setText("预览失败："+error);return;}
        preview_->setImage(QPixmap::fromImage(image));preview_->setDragMode(QGraphicsView::NoDrag);
        selection_->setBounds(preview_->sceneRect());selection_->setRatio(ratio_->currentData().toDouble());
        if(!initialized_ && initial_.crop!=cv::Rect2d()) {
            const auto b=preview_->sceneRect();const auto r=initial_.crop;
            selection_->setSelection(QRectF(r.x*b.width(),r.y*b.height(),r.width*b.width(),r.height*b.height()));
        }
        initialized_=true;
        if(!sharedPreview_)preview_->fitToWindow();
        updateSize();
    });
    updateImage();
}
void GeometryDialog::updateImage()
{
    auto base=working_;base.crop={};base.targetSize={};base.background=ImageProcessor::BackgroundMode::None;
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);selection_->setVisible(false);
    task_.submit(previewSource_,base);
}
void GeometryDialog::updateSize()
{
    const QRectF bounds=preview_->sceneRect();
    if(bounds.isEmpty()) return;
    const auto size=ImageProcessor::geometrySize(cv::Size(originalSize_.width(),originalSize_.height()),options());
    const int width=size.width,height=size.height;
    sizeLabel_->setText(QString(sharedPreview_?"原图：%1 × %2 px\n裁剪目标：%3 × %4 px"
                                            :"原图：%1 × %2 px    裁剪目标：%3 × %4 px")
        .arg(originalSize_.width()).arg(originalSize_.height()).arg(width).arg(height));
    selection_->setVisible(initialized_ && !task_.isBusy());
    const bool valid=initialized_ && !task_.isBusy() && ImageProcessor::validOutputSize(size);
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(valid);
}
ImageProcessor::Options GeometryDialog::options() const
{
    auto result=working_;
    const QRectF b=preview_->sceneRect(), r=selection_->selection();
    if(!initialized_ || b.isEmpty())return working_;
    result.crop=cv::Rect2d(r.x()/b.width(),r.y()/b.height(),r.width()/b.width(),r.height()/b.height());
    if(result.crop==cv::Rect2d(0,0,1,1))result.crop={};
    const auto sameCrop = [](const cv::Rect2d &a,const cv::Rect2d &b) {
        return std::abs(a.x-b.x)<1e-9 && std::abs(a.y-b.y)<1e-9
            && std::abs(a.width-b.width)<1e-9 && std::abs(a.height-b.height)<1e-9;
    };
    if(result.rotation==initial_.rotation && result.flipHorizontal==initial_.flipHorizontal
        && result.flipVertical==initial_.flipVertical && sameCrop(result.crop,initial_.crop))
        return initial_;
    result.targetSize={};
    result.strokes.clear(); // 几何坐标改变后，需要重新修正背景笔触。
    return result;
}
