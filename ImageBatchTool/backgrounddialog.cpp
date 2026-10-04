#include "backgrounddialog.h"
#include "dialogappearance.h"
#include "selectionitem.h"
#include "imageprocessing.h"
#include "sliderstyle.h"
#include <QtConcurrent>
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QIcon>
#include <exception>

void BrushPreview::append(QPoint point) {
    const QRectF b=sceneRect();
    const QPointF p=mapToScene(point);
    if(!b.contains(p))return;
    stroke_.points.emplace_back((p.x()-b.x())/b.width(),(p.y()-b.y())/b.height());
    // 反馈本次画笔轨迹，结束后由结果预览替换。
    const double r=radius*std::min(b.width(),b.height());
    auto *mark=scene()->addEllipse(p.x()-r,p.y()-r,2*r,2*r,QPen(Qt::NoPen),
                                  QColor(foreground?QColor(30,180,100,110):QColor(230,60,80,110)));
    mark->setData(0,"brushMark");mark->setZValue(10);
}
void BrushPreview::mousePressEvent(QMouseEvent *e) {
    if(painting && e->button()==Qt::LeftButton && sceneRect().contains(mapToScene(e->position().toPoint()))) {
        stroke_={};stroke_.foreground=foreground;stroke_.radius=radius;drawing_=true;
        append(e->position().toPoint());e->accept();return;
    }
    PreviewLabel::mousePressEvent(e);
}
void BrushPreview::mouseMoveEvent(QMouseEvent *e) {
    if(drawing_){append(e->position().toPoint());e->accept();return;}
    PreviewLabel::mouseMoveEvent(e);
}
void BrushPreview::mouseReleaseEvent(QMouseEvent *e) {
    if(drawing_ && e->button()==Qt::LeftButton) {
        append(e->position().toPoint());drawing_=false;
        for(auto *item:scene()->items())if(item->data(0).toString()=="brushMark")delete item;
        if(!stroke_.points.empty() && onStroke)onStroke(std::move(stroke_));
        e->accept();return;
    }
    PreviewLabel::mouseReleaseEvent(e);
}
BackgroundDialog::BackgroundDialog(const QImage &original,const ImageProcessor::Options &options,QWidget *parent)
    : QDialog(parent),working_(options),preview_(new BrushPreview(this)),selection_(new SelectionItem),
      buttons_(new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this)),message_(new QLabel(this))
{
    setObjectName("BackgroundDialog");resize(1040,760);setMinimumSize(820,640);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(12,12,12,12);root->setSpacing(0);
    auto *content=new QWidget(this);content->setObjectName("contentPanel");root->addWidget(content,1);
    auto *body=new QVBoxLayout(content);body->setContentsMargins(24,20,24,20);body->setSpacing(14);
    auto *heading=new QLabel("背景编辑",this);heading->setObjectName("dialogHeading");
    auto *head=new QHBoxLayout;head->addWidget(heading,1);
    auto *close=new QToolButton(this);close->setObjectName("closeButton");close->setIcon(QIcon(":/indicators/close.svg"));
    close->setFixedSize(32,32);head->addWidget(close);body->addLayout(head);
    auto *hint=new QLabel("先调整主体范围，再用“保留主体 / 删除背景”画笔修正。滚轮可缩放；浏览模式可拖动。",this);
    hint->setWordWrap(true);hint->setObjectName("modeHint");body->addWidget(hint);
    auto *modes=new QHBoxLayout;
    auto *mode=new QComboBox(this);mode->setObjectName("backgroundMode");
    mode->addItems({"模糊背景","移除背景（透明）","替换背景"});
    mode->setCurrentIndex(options.background==ImageProcessor::BackgroundMode::Remove?1:
                          options.background==ImageProcessor::BackgroundMode::Replace?2:0);
    working_.background=static_cast<ImageProcessor::BackgroundMode>(mode->currentIndex()+1);
    modes->addWidget(mode);
    auto *color=new QPushButton("选择背景颜色",this);color->setAutoDefault(false);modes->addWidget(color);
    auto *file=new QPushButton("选择背景图片",this);file->setAutoDefault(false);modes->addWidget(file);
    auto *reset=new QPushButton("重置背景",this);reset->setObjectName("resetBackgroundButton");reset->setAutoDefault(false);
    modes->addWidget(reset);modes->addStretch();body->addLayout(modes);
    preview_->setObjectName("backgroundPreview");preview_->setCornerRadius(8);
    selection_->setParent(this);preview_->scene()->addItem(selection_);body->addWidget(preview_,1);
    auto *tools=new QHBoxLayout;
    auto *brushMode=new QComboBox(this);brushMode->setObjectName("brushMode");
    brushMode->addItems({"调整主体范围","保留主体画笔","删除背景画笔","浏览结果"});tools->addWidget(brushMode);
    tools->addWidget(new QLabel("笔刷大小",this));
    auto *radius=new QSlider(Qt::Horizontal,this);radius->setObjectName("brushRadiusSlider");
    radius->setRange(1,100);radius->setValue(25);AbsoluteSliderStyle::applyTo(radius);tools->addWidget(radius,1);
    auto *undoStroke=new QPushButton("撤销笔触",this);undoStroke->setAutoDefault(false);tools->addWidget(undoStroke);body->addLayout(tools);
    auto *effects=new QHBoxLayout;effects->addWidget(new QLabel("边缘柔和度",this));
    auto *feather=new QSlider(Qt::Horizontal,this);feather->setObjectName("featherSlider");
    feather->setRange(0,20);feather->setValue(working_.feather);AbsoluteSliderStyle::applyTo(feather);effects->addWidget(feather,1);
    effects->addWidget(new QLabel("模糊强度",this));auto *blur=new QSlider(Qt::Horizontal,this);
    blur->setRange(1,50);blur->setValue(working_.backgroundBlur);AbsoluteSliderStyle::applyTo(blur);effects->addWidget(blur,1);
    body->addLayout(effects);message_->setWordWrap(true);body->addWidget(message_);
    auto *footer=new QWidget(this);footer->setObjectName("footerPanel");root->addWidget(footer);
    auto *foot=new QHBoxLayout(footer);foot->setContentsMargins(24,16,24,16);foot->addWidget(buttons_);
    DialogAppearance::setup(this,{heading});DialogAppearance::setupButtons(buttons_);
    connect(close,&QToolButton::clicked,this,&QDialog::reject);
    connect(buttons_,&QDialogButtonBox::accepted,this,&QDialog::accept);
    connect(buttons_,&QDialogButtonBox::rejected,this,&QDialog::reject);
    debounce_.setSingleShot(true);debounce_.setInterval(150);
    connect(&debounce_,&QTimer::timeout,this,&BackgroundDialog::startPreview);
    connect(&watcher_,&QFutureWatcher<PreviewResult>::finished,this,[this] {
        if(dirty_){startPreview();return;}
        const auto result=watcher_.result();
        if(result.error.isEmpty()) {
            if(!preview_->painting && !selection_->isVisible())preview_->setImage(QPixmap::fromImage(result.image));
            message_->setText("预览已更新。调整主体范围时框外必须包含背景；确定后应用到完整图片。");
        }else message_->setText("预览失败："+result.error);
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(result.error.isEmpty());
    });
    const auto setBrushMode=[this](int index) {
        selection_->setVisible(index==0);preview_->painting=index==1||index==2;preview_->foreground=index==1;
        if(index!=3)preview_->setImage(QPixmap::fromImage(base_));
        preview_->setDragMode(preview_->painting?QGraphicsView::NoDrag:QGraphicsView::ScrollHandDrag);
        preview_->viewport()->setCursor(preview_->painting?Qt::CrossCursor:Qt::ArrowCursor);
        schedulePreview();
    };
    connect(brushMode,&QComboBox::currentIndexChanged,this,setBrushMode);
    connect(mode,&QComboBox::currentIndexChanged,this,[this,color,file,blur](int index) {
        working_.background=static_cast<ImageProcessor::BackgroundMode>(index+1);
        color->setEnabled(index==2);file->setEnabled(index==2);blur->setEnabled(index==0);schedulePreview();
    });
    color->setEnabled(mode->currentIndex()==2);file->setEnabled(mode->currentIndex()==2);blur->setEnabled(mode->currentIndex()==0);
    connect(color,&QPushButton::clicked,this,[this] {
        const QColor c=QColorDialog::getColor(QColor(working_.backgroundColor[0],working_.backgroundColor[1],working_.backgroundColor[2]),this);
        if(c.isValid()){working_.backgroundColor=cv::Scalar(c.red(),c.green(),c.blue());working_.backgroundImage.release();schedulePreview();}
    });
    connect(file,&QPushButton::clicked,this,[this] {
        const QString path=QFileDialog::getOpenFileName(this,"选择背景图片",{},"图片 (*.png *.jpg *.jpeg *.bmp)");
        if(path.isEmpty())return;
        const QImage image=QImage(path).convertToFormat(QImage::Format_RGB888);
        if(image.isNull()){message_->setText("无法读取背景图片。");return;}
        working_.backgroundImage=cv::Mat(image.height(),image.width(),CV_8UC3,const_cast<uchar*>(image.constBits()),image.bytesPerLine()).clone();
        schedulePreview();
    });
    connect(radius,&QSlider::valueChanged,this,[this](int value){preview_->radius=value/1000.0;});
    connect(feather,&QSlider::valueChanged,this,[this](int value){working_.feather=value;schedulePreview();});
    connect(blur,&QSlider::valueChanged,this,[this](int value){working_.backgroundBlur=value;schedulePreview();});
    connect(undoStroke,&QPushButton::clicked,this,[this]{if(!working_.strokes.empty()){working_.strokes.pop_back();schedulePreview();}});
    preview_->onStroke=[this](auto stroke){working_.strokes.push_back(std::move(stroke));schedulePreview();};
    connect(selection_,&SelectionItem::selectionChanged,this,[this] {
        const auto b=preview_->sceneRect(),r=selection_->selection();if(b.isEmpty())return;
        working_.foregroundRect=cv::Rect2d(r.x()/b.width(),r.y()/b.height(),r.width()/b.width(),r.height()/b.height());schedulePreview();
    });
    connect(reset,&QPushButton::clicked,this,[this] {
        working_.strokes.clear();working_.backgroundImage.release();working_.backgroundColor=cv::Scalar::all(255);
        const auto b=preview_->sceneRect();selection_->setSelection(QRectF(b.width()*.1,b.height()*.05,b.width()*.8,b.height()*.9));schedulePreview();
    });
    try {
        auto base=options;base.background=ImageProcessor::BackgroundMode::None;base.clearTone();base.grayscale=false;
        if(base.targetSize!=cv::Size()) {
            const QSize size=QSize(base.targetSize.width,base.targetSize.height).scaled(1024,1024,Qt::KeepAspectRatio);
            base.targetSize=cv::Size(size.width(),size.height());
        }
        const QImage small=original.width()>1024||original.height()>1024?original.scaled(1024,1024,Qt::KeepAspectRatio,Qt::SmoothTransformation):original;
        base_=ImageProcessing::processImage(small,base);
        const auto r=working_.foregroundRect;
        preview_->setImage(QPixmap::fromImage(base_));selection_->setBounds(preview_->sceneRect());
        selection_->setSelection(QRectF(r.x*base_.width(),r.y*base_.height(),r.width*base_.width(),r.height*base_.height()));
        preview_->fitToWindow();brushMode->setCurrentIndex(3);schedulePreview();
    }catch(const std::exception &e){message_->setText(QString::fromUtf8(e.what()));buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);}
}
BackgroundDialog::~BackgroundDialog(){watcher_.waitForFinished();}
void BackgroundDialog::schedulePreview() {
    if(base_.isNull())return;
    dirty_=true;buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);debounce_.start();
    message_->setText("正在更新预览…");
}
void BackgroundDialog::startPreview() {
    if(watcher_.isRunning())return;
    dirty_=false;
    auto options=working_;options.rotation=0;options.flipHorizontal=options.flipVertical=false;options.crop={};options.targetSize={};
    const QImage source=base_;
    watcher_.setFuture(QtConcurrent::run([source,options] {
        PreviewResult result;
        try{result.image=ImageProcessing::processImage(source,options);}catch(const std::exception &e){result.error=QString::fromUtf8(e.what());}
        return result;
    }));
}
