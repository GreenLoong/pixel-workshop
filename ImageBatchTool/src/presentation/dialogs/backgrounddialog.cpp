#include "presentation/dialogs/backgrounddialog.h"
#include "presentation/widgets/dialogappearance.h"
#include "presentation/widgets/selectionitem.h"
#include "application/imageprocessing.h"
#include "infrastructure/imageconversion.h"
#include "presentation/widgets/previewimage.h"
#include "presentation/widgets/sliderstyle.h"
#include <QtConcurrent>
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QIcon>
#include <QCheckBox>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <exception>
#include <QPainter>

BackgroundDialog::BackgroundDialog(const QImage &original,const ImageProcessor::Options &options,QWidget *parent,BrushPreview *sharedPreview)
    : QDialog(parent),working_(options),preview_(sharedPreview?sharedPreview:new BrushPreview(this)),selection_(new SelectionItem),
      buttons_(new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this)),message_(new QLabel(this)),sharedPreview_(sharedPreview!=nullptr)
{
    setObjectName("BackgroundDialog");resize(1040,760);setMinimumSize(820,640);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(12,12,12,12);root->setSpacing(0);
    auto *content=new QWidget(this);content->setObjectName("contentPanel");root->addWidget(content,1);
    auto *body=new QVBoxLayout(content);body->setContentsMargins(24,20,24,20);body->setSpacing(14);
    auto *heading=new QLabel("背景编辑",this);heading->setObjectName("dialogHeading");
    auto *head=new QHBoxLayout;head->addWidget(heading,1);
    auto *close=new QToolButton(this);close->setObjectName("closeButton");close->setIcon(QIcon(":/indicators/close.svg"));
    close->setFixedSize(32,32);head->addWidget(close);body->addLayout(head);
    auto *hint=new QLabel("人像模式自动识别人物与衣物；非人像请选择通用区域。画笔可修正保留 / 删除区域，滚轮可缩放。",this);
    hint->setWordWrap(true);hint->setObjectName("modeHint");body->addWidget(hint);
    auto *editor=new QHBoxLayout;editor->setSpacing(18);body->addLayout(editor,1);
    if(!sharedPreview_) {
        preview_->setObjectName("backgroundPreview");preview_->setCornerRadius(8);editor->addWidget(preview_,1);
    }
    selection_->setParent(this);preview_->scene()->addItem(selection_);
    auto *panel=new QWidget(this);auto *rows=new QVBoxLayout(panel);rows->setSpacing(16);
    rows->setContentsMargins(8,8,8,8);
    if(sharedPreview_)editor->addWidget(panel);
    else {
        auto *scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setFixedWidth(320);scroll->setWidget(panel);editor->addWidget(scroll);
    }
    rows->addWidget(new QLabel("背景效果",this));
    auto *modes=new QVBoxLayout;
    auto *mode=new QComboBox(this);mode->setObjectName("backgroundMode");
    mode->addItems({"不处理背景","模糊背景","移除背景（透明）","替换背景"});
    mode->setCurrentIndex(static_cast<int>(options.background));
    modes->addWidget(mode);
    auto *color=new QPushButton("选择背景颜色",this);color->setAutoDefault(false);modes->addWidget(color);
    auto *file=new QPushButton("选择背景图片",this);file->setAutoDefault(false);modes->addWidget(file);
    auto *reset=new QPushButton("重置背景",this);reset->setObjectName("resetBackgroundButton");reset->setAutoDefault(false);
    rows->addLayout(modes);
    auto *engineRow=new QVBoxLayout;engineRow->addWidget(new QLabel("主体识别",this));
    auto *engine=new QComboBox(this);engine->setObjectName("segmentationMethod");
    engine->addItems({"人像（自动识别人物）","通用区域（手工选择范围）"});
    engine->setCurrentIndex(options.segmentation==ImageProcessor::SegmentationMethod::Human?0:1);
    engineRow->addWidget(engine);rows->addLayout(engineRow);
    auto *range=new QPushButton("调整主体范围",this);range->setObjectName("rangeSelectionButton");
    range->setCheckable(true);range->setAutoDefault(false);rows->addWidget(range);
    auto *brushEnabled=new QCheckBox("手工修正",this);brushEnabled->setObjectName("brushEnabled");rows->addWidget(brushEnabled);
    auto *brushPanel=new QWidget(this);brushPanel->setObjectName("brushPanel");
    auto *tools=new QVBoxLayout(brushPanel);tools->setContentsMargins(0,0,0,0);tools->setSpacing(12);rows->addWidget(brushPanel);
    auto *keep=new QRadioButton("保留主体",this);keep->setObjectName("keepBrush");keep->setChecked(true);
    auto *remove=new QRadioButton("删除背景",this);remove->setObjectName("removeBrush");
    auto *brushGroup=new QButtonGroup(this);brushGroup->addButton(keep);brushGroup->addButton(remove);
    auto *brushChoice=new QHBoxLayout;brushChoice->addWidget(keep);brushChoice->addWidget(remove);tools->addLayout(brushChoice);
    auto *radiusLabel=new QLabel(this);tools->addWidget(radiusLabel);
    auto *radius=new QSlider(Qt::Horizontal,this);radius->setObjectName("brushRadiusSlider");
    radius->setRange(1,100);radius->setValue(25);AbsoluteSliderStyle::applyTo(radius);tools->addWidget(radius);
    auto *undoStroke=new QPushButton("撤销笔触",this);undoStroke->setObjectName("undoStrokeButton");undoStroke->setAutoDefault(false);tools->addWidget(undoStroke);
    auto *effects=new QVBoxLayout;auto *featherLabel=new QLabel(this);effects->addWidget(featherLabel);
    auto *feather=new QSlider(Qt::Horizontal,this);feather->setObjectName("featherSlider");
    feather->setRange(0,20);feather->setValue(working_.feather);AbsoluteSliderStyle::applyTo(feather);effects->addWidget(feather,1);
    rows->addLayout(effects);
    auto *blurPanel=new QWidget(this);blurPanel->setObjectName("blurPanel");
    auto *blurLayout=new QVBoxLayout(blurPanel);blurLayout->setContentsMargins(0,0,0,0);
    auto *blurLabel=new QLabel(this);blurLayout->addWidget(blurLabel);
    auto *blur=new QSlider(Qt::Horizontal,this);blur->setObjectName("backgroundBlurSlider");
    blur->setRange(1,50);blur->setValue(working_.backgroundBlur);AbsoluteSliderStyle::applyTo(blur);blurLayout->addWidget(blur);
    rows->addWidget(blurPanel);rows->addWidget(reset);rows->addStretch();
    message_->setWordWrap(true);message_->setObjectName("backgroundMessage");body->addWidget(message_);
    const auto updateValues=[=] {
        radiusLabel->setText(QString("画笔直径 · 图片短边的 %1%").arg(radius->value()/5.0,0,'f',1));
        featherLabel->setText(QString("边缘柔和度   %1").arg(feather->value()));
        blurLabel->setText(QString("模糊强度   %1").arg(blur->value()));
    };
    for(auto *slider:{radius,feather,blur})connect(slider,&QSlider::valueChanged,this,updateValues);
    updateValues();
    auto *footer=new QWidget(this);footer->setObjectName("footerPanel");root->addWidget(footer);
    auto *foot=new QHBoxLayout(footer);foot->setContentsMargins(24,16,24,16);foot->addWidget(buttons_);
    DialogAppearance::setup(this,{heading});DialogAppearance::setupButtons(buttons_);
    connect(close,&QToolButton::clicked,this,&QDialog::reject);
    connect(buttons_,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);
    debounce_.setSingleShot(true);debounce_.setInterval(150);
    connect(&debounce_,&QTimer::timeout,this,&BackgroundDialog::startPreview);
    connect(&watcher_,&QFutureWatcher<BackgroundPreview::Result>::finished,this,[this] {
        if(dirty_){startPreview();return;}
        const auto result=watcher_.result();
        if(result.error.isEmpty()) {
            result_=result.image;mask_=result.mask;presentPreview();
            message_->setText(!result.notice.isEmpty()?result.notice:working_.segmentation==ImageProcessor::SegmentationMethod::Human
                ? "人像识别已更新，可用画笔修正边缘；确定后应用到完整图片。"
                : "区域分割已更新。范围框外作为背景，主体必须完整包含在框内。");
        }else message_->setText("预览失败："+result.error);
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(result.error.isEmpty());
    });
    const auto updateTool=[this,range,brushEnabled,brushPanel,keep] {
        selection_->setVisible(range->isChecked() && working_.segmentation==ImageProcessor::SegmentationMethod::Region);
        preview_->painting=brushEnabled->isChecked();preview_->foreground=keep->isChecked();
        brushPanel->setVisible(preview_->painting);presentPreview();schedulePreview();
    };
    connect(range,&QPushButton::toggled,this,[brushEnabled,updateTool](bool checked) {
        if(checked){const QSignalBlocker block(brushEnabled);brushEnabled->setChecked(false);}updateTool();
    });
    connect(brushEnabled,&QCheckBox::toggled,this,[range,updateTool](bool checked) {
        if(checked){const QSignalBlocker block(range);range->setChecked(false);}updateTool();
    });
    connect(keep,&QRadioButton::toggled,this,updateTool);
    const auto setEngine=[this,engine,range,brushEnabled,updateTool] {
        const bool human=engine->currentIndex()==0;
        working_.segmentation=human?ImageProcessor::SegmentationMethod::Human:ImageProcessor::SegmentationMethod::Region;
        range->setVisible(!human);range->setChecked(!human);
        if(!human)brushEnabled->setChecked(false);
        updateTool();schedulePreview();
    };
    connect(engine,&QComboBox::currentIndexChanged,this,[setEngine]{setEngine();});
    connect(mode,&QComboBox::currentIndexChanged,this,[this,color,file,blurPanel](int index) {
        working_.background=static_cast<ImageProcessor::BackgroundMode>(index);
        color->setVisible(index==3);file->setVisible(index==3);blurPanel->setVisible(index==1);schedulePreview();
    });
    color->setObjectName("backgroundColorButton");file->setObjectName("backgroundFileButton");
    color->setVisible(mode->currentIndex()==3);file->setVisible(mode->currentIndex()==3);blurPanel->setVisible(mode->currentIndex()==1);
    connect(color,&QPushButton::clicked,this,[this] {
        const QColor c=QColorDialog::getColor(QColor(working_.backgroundColor[0],working_.backgroundColor[1],working_.backgroundColor[2]),this,"选择背景颜色",QColorDialog::DontUseNativeDialog);
        if(c.isValid()){working_.backgroundColor=cv::Scalar(c.red(),c.green(),c.blue());working_.backgroundImage.release();schedulePreview();}
    });
    connect(file,&QPushButton::clicked,this,[this] {
        const QString path=QFileDialog::getOpenFileName(this,"选择背景图片",{},"图片 (*.png *.jpg *.jpeg *.bmp)");
        if(path.isEmpty())return;
        const QImage image=QImage(path).convertToFormat(QImage::Format_RGB888);
        if(image.isNull()){message_->setText("无法读取背景图片。");return;}
        working_.backgroundImage=ImageConversion::rgbView(image).clone();
        schedulePreview();
    });
    connect(radius,&QSlider::valueChanged,this,[this](int value){preview_->radius=value/1000.0;preview_->viewport()->update();});
    connect(feather,&QSlider::valueChanged,this,[this](int value){working_.feather=value;schedulePreview();});
    connect(blur,&QSlider::valueChanged,this,[this](int value){working_.backgroundBlur=value;schedulePreview();});
    connect(undoStroke,&QPushButton::clicked,this,[this]{if(!working_.strokes.empty()){working_.strokes.pop_back();schedulePreview();}});
    preview_->onStroke=[this](auto stroke){working_.strokes.push_back(std::move(stroke));schedulePreview();};
    connect(selection_,&SelectionItem::selectionChanged,this,[this] {
        const QRectF b(QPointF(),base_.size());const auto r=selection_->selection();if(b.isEmpty())return;
        working_.foregroundRect=cv::Rect2d(r.x()/b.width(),r.y()/b.height(),r.width()/b.width(),r.height()/b.height());schedulePreview();
    });
    connect(reset,&QPushButton::clicked,this,[this] {
        working_.strokes.clear();working_.backgroundImage.release();working_.backgroundColor=cv::Scalar::all(255);
        const auto b=preview_->sceneRect();selection_->setSelection(QRectF(b.width()*.1,b.height()*.05,b.width()*.8,b.height()*.9));schedulePreview();
    });
    try {
        const int limit=sharedPreview_?PreviewImage::MaxExtent:1024;
        auto base=options;base.background=ImageProcessor::BackgroundMode::None;base.clearTone();base.grayscale=false;
        if(base.targetSize!=cv::Size()) {
            const QSize actual(base.targetSize.width,base.targetSize.height);
            const QSize size=PreviewImage::boundedSize(actual,limit);
            base.targetSize=cv::Size(size.width(),size.height());
        }
        const QImage small=PreviewImage::thumbnail(original,limit);
        base_=ImageProcessing::processImage(small,base);
        const auto r=working_.foregroundRect;
        if(!sharedPreview_)preview_->setImage(QPixmap::fromImage(base_));
        selection_->setBounds(QRectF(QPointF(),base_.size()));
        selection_->setSelection(QRectF(r.x*base_.width(),r.y*base_.height(),r.width*base_.width(),r.height*base_.height()));
        if(!sharedPreview_)preview_->fitToWindow();
        setEngine();range->setChecked(false);initializing_=false;updateTool();schedulePreview();
    }catch(const std::exception &e){message_->setText(QString::fromUtf8(e.what()));buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);}
}
BackgroundDialog::~BackgroundDialog(){watcher_.waitForFinished();preview_->onStroke={};preview_->painting=false;}
void BackgroundDialog::schedulePreview() {
    emit optionsChanged();
    if(base_.isNull())return;
    dirty_=true;buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);debounce_.start();
    message_->setText("正在更新预览…");
}
void BackgroundDialog::presentPreview()
{
    if(sharedPreview_ && initializing_)return;
    if(sharedPreview_ && result_.isNull() && !selection_->isVisible() && !preview_->painting)return;
    QImage image=result_.isNull()?base_:result_;
    if(selection_->isVisible())image=base_;
    else if(preview_->painting && !mask_.isNull()) {
        // 原图上以蓝色蒙版标出已删除区域，仍能看见被误删的衣物并补回。
        image=base_.convertToFormat(QImage::Format_ARGB32);
        QImage mask(mask_.size(),QImage::Format_ARGB32);
        for(int y=0;y<mask.height();++y) {
            auto *line=reinterpret_cast<QRgb *>(mask.scanLine(y));
            for(int x=0;x<mask.width();++x)line[x]=qRgba(70,140,245,(255-mask_.pixelColor(x,y).alpha())/2);
        }
        QPainter p(&image);p.drawImage(0,0,mask);
    }
    preview_->setImage(QPixmap::fromImage(image));
    preview_->setDragMode(preview_->painting || selection_->isVisible()?QGraphicsView::NoDrag:QGraphicsView::ScrollHandDrag);
    preview_->viewport()->setCursor(preview_->painting?Qt::CrossCursor:Qt::ArrowCursor);
    preview_->viewport()->update();
}
void BackgroundDialog::startPreview() {
    if(watcher_.isRunning())return;
    dirty_=false;
    auto options=working_;options.rotation=0;options.flipHorizontal=options.flipVertical=false;options.crop={};options.targetSize={};
    const QImage source=base_;
    const bool needsMask=preview_->painting;
    watcher_.setFuture(QtConcurrent::run([source,options,needsMask] {
        return BackgroundPreview::render(source,options,needsMask);
    }));
}
