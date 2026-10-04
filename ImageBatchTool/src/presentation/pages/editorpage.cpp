#include "presentation/pages/editorpage.h"
#include "presentation/dialogs/geometrydialog.h"
#include "presentation/dialogs/tonedialog.h"
#include "presentation/dialogs/resizedialog.h"
#include "presentation/dialogs/backgrounddialog.h"
#include "presentation/widgets/dialogappearance.h"
#include "application/imagetask.h"
#include "presentation/widgets/selectionitem.h"
#include "presentation/widgets/brushpreview.h"
#include "presentation/widgets/previewimage.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QShortcut>
#include <exception>

EditorPage::EditorPage(QWidget *parent):QWidget(parent),host_(new QWidget(this)),
    body_(new QVBoxLayout(host_)),preview_(new BrushPreview(this)),modes_(new QButtonGroup(this))
{
    setObjectName("editorPage");
    QString theme;
    for(const char *path:{":/styles/dialog.qss",":/styles/editor.qss"}) {
        QFile sheet(path);
        if(sheet.open(QIODevice::ReadOnly))theme+=QString::fromUtf8(sheet.readAll());
    }
    setStyleSheet(theme);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(20,10,20,16);root->setSpacing(14);
    auto *top=new QHBoxLayout;root->addLayout(top);
    name_=new QLabel(this);name_->setObjectName("editorFileName");top->addWidget(name_,1);
    name_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    auto *reset=new QPushButton("恢复原图",this);reset->setObjectName("resetEditorButton");top->addWidget(reset);
    connect(reset,&QPushButton::clicked,this,[this]{record();draft_={};mode_=Crop;buildPanel();record();});
    auto *cancel=new QPushButton("取消",this);cancel->setObjectName("cancelEditButton");
    done_=new QPushButton("完成编辑",this);done_->setObjectName("confirmButton");
    top->addWidget(cancel);top->addWidget(done_);
    auto *toolbar=new QHBoxLayout;root->addLayout(toolbar);
    const auto tool=[&](const QString &text,const QString &name,auto callback) {
        auto *b=new QPushButton(text,this);b->setObjectName(name);b->setAutoDefault(false);
        b->setProperty("modeButton",true);toolbar->addWidget(b);connect(b,&QPushButton::clicked,this,callback);return b;
    };
    tool("−","editorZoomOut",[this]{if(preview_)preview_->zoomOut();});
    zoom_=new QLabel("100%",this);zoom_->setFixedWidth(52);zoom_->setAlignment(Qt::AlignCenter);toolbar->addWidget(zoom_);
    tool("+","editorZoomIn",[this]{if(preview_)preview_->zoomIn();});
    tool("适应 / 1:1","editorFit",[this]{if(preview_)preview_->toggleFitActual();});
    undo_=tool("撤销","editorUndo",[this]{undo();});redo_=tool("重做","editorRedo",[this]{redo();});
    toolbar->addStretch();
    int id=0;
    for(const QString &title:{QString("裁剪旋转"),QString("颜色光线"),QString("尺寸"),QString("背景")}) {
        auto *b=new QPushButton(title,this);b->setObjectName(QString("editorMode%1").arg(id));
        b->setProperty("modeButton",true);b->setCheckable(true);modes_->addButton(b,id++);toolbar->addWidget(b);
    }
    body_->setContentsMargins(10,12,10,12);body_->setAlignment(Qt::AlignTop);
    auto *workspace=new QHBoxLayout;workspace->setSpacing(18);root->addLayout(workspace,1);
    preview_->setObjectName("editorPreview");preview_->setKeepViewOnImageChange(true);
    preview_->setStyleSheet("QGraphicsView {background:#202124; border:1px solid #363a43; border-radius:10px;}");
    preview_->setBackgroundBrush(QColor("#202124"));preview_->setCornerRadius(10);
    workspace->addWidget(preview_,1);
    auto *parameters=new QScrollArea(this);parameters->setObjectName("editorParameters");
    parameters->setFrameShape(QFrame::NoFrame);parameters->setWidgetResizable(true);parameters->setFixedWidth(320);
    parameters->setWidget(host_);workspace->addWidget(parameters);
    connect(preview_,&PreviewLabel::zoomChanged,this,[this](int value){zoom_->setText(QString::number(value)+"%");});
    connect(modes_,&QButtonGroup::idClicked,this,[this](int id){selectMode(static_cast<Mode>(id));});
    connect(cancel,&QPushButton::clicked,this,&EditorPage::cancelled);
    connect(done_,&QPushButton::clicked,this,[this]{record();if(valid())emit accepted();});
    auto *escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);connect(escape,&QShortcut::activated,this,&EditorPage::cancelled);
    recordTimer_.setSingleShot(true);recordTimer_.setInterval(250);
    connect(&recordTimer_,&QTimer::timeout,this,&EditorPage::record);
}
EditorPage::~EditorPage(){end();} // 参数控制器先于共享画布释放。
void EditorPage::begin(const QImage &image,const ImageProcessor::Options &options,const QString &name,Mode mode)
{
    recordTimer_.stop();original_=image;draft_=options;mode_=mode;
    name_->setText("编辑图片 · "+name);name_->setToolTip(name);
    history_={{options,mode}};index_=0;buildPanel();
}
void EditorPage::end()
{
    recordTimer_.stop();panel_=nullptr;
    while(auto *item=body_->takeAt(0)){delete item->widget();delete item;}
    original_={};draft_={};history_.clear();index_=0;
    preview_->setImage(QPixmap());preview_->fitToWindow();
}
ImageProcessor::Options EditorPage::options() const
{
    if(auto *p=qobject_cast<GeometryDialog *>(panel_))return p->options();
    if(auto *p=qobject_cast<ToneDialog *>(panel_))return p->options();
    if(auto *p=qobject_cast<BackgroundDialog *>(panel_))return p->options();
    auto result=draft_;
    if(auto *p=qobject_cast<ResizeDialog *>(panel_)) {
        const auto size=p->targetSize();
        // 没有改动尺寸时，保留空 targetSize 的原始语义。
        if(size!=p->property("entrySize").toSize())result.targetSize=cv::Size(size.width(),size.height());
    }
    return result;
}
bool EditorPage::valid() const
{
    auto *box=panel_?panel_->findChild<QDialogButtonBox *>():nullptr;
    return box && box->button(QDialogButtonBox::Ok)->isEnabled();
}
void EditorPage::updateButtons()
{
    const bool pending=panel_ && !history_.empty() && !(options()==history_[index_].options);
    done_->setEnabled(valid());undo_->setEnabled(index_>0 || pending);
    redo_->setEnabled(!pending && index_+1<static_cast<int>(history_.size()));
}
bool EditorPage::eventFilter(QObject *object,QEvent *event)
{
    if(event->type()==QEvent::EnabledChange)updateButtons();
    // 嵌入的 QDialog 不可用 Enter 自行 accept 并隐藏整块面板。
    if(object==panel_ && event->type()==QEvent::KeyPress) {
        const auto key=static_cast<QKeyEvent *>(event)->key();
        if(key==Qt::Key_Return || key==Qt::Key_Enter)return true;
    }
    return QWidget::eventFilter(object,event);
}
void EditorPage::record()
{
    recordTimer_.stop();
    if(!panel_ || history_.empty())return;
    const auto state=options();
    if(!(state==history_[index_].options)) {
        history_.resize(index_+1);history_.push_back({state,mode_});
        if(history_.size()>51)history_.erase(history_.begin());
        index_=static_cast<int>(history_.size())-1;
    }
    updateButtons();
}
void EditorPage::selectMode(Mode mode)
{
    if(mode==mode_)return;
    if(!valid()) {modes_->button(mode_)->setChecked(true);return;}
    record();draft_=options();
    const bool showingResult=mode_==Tone || mode_==Resize
        || (mode_==Background && !preview_->painting && !panel_->findChild<SelectionItem *>()->isVisible())
        || (mode_==Crop && draft_.crop==cv::Rect2d() && draft_.targetSize==cv::Size()
            && draft_.background==ImageProcessor::BackgroundMode::None);
    const bool reusePreview=mode==Resize && showingResult;
    mode_=mode;buildPanel(reusePreview);
}
void EditorPage::undo()
{
    record();if(index_==0)return;
    --index_;draft_=history_[index_].options;mode_=history_[index_].mode;buildPanel();
}
void EditorPage::redo()
{
    if(index_+1>=static_cast<int>(history_.size()))return;
    ++index_;draft_=history_[index_].options;mode_=history_[index_].mode;buildPanel();
}
void EditorPage::buildPanel(bool reusePreview)
{
    recordTimer_.stop();
    // 只替换参数面板；常驻画布不隐藏、不重新创建，也不排入一次延迟缩放。
    setUpdatesEnabled(false);
    panel_=nullptr;
    while(auto *item=body_->takeAt(0)){delete item->widget();delete item;}
    preview_->painting=false;preview_->onStroke={};preview_->viewport()->unsetCursor();
    preview_->setDragMode(QGraphicsView::ScrollHandDrag);
    if(mode_==Crop)panel_=new GeometryDialog(original_,draft_,host_,preview_);
    else if(mode_==Tone)panel_=new ToneDialog(original_,draft_,host_,preview_);
    else if(mode_==Background)panel_=new BackgroundDialog(original_,draft_,host_,preview_);
    else {
        QSize current=original_.size();
        // 尺寸只需要几何结果；跳过分割和调色，避免在切页时重复昂贵处理。
        if(draft_.targetSize!=cv::Size())current=QSize(draft_.targetSize.width,draft_.targetSize.height);
        else {
            const auto size=ImageProcessor::geometrySize(cv::Size(original_.width(),original_.height()),draft_);
            current=QSize(size.width,size.height);
        }
        auto *resize=new ResizeDialog(original_.size(),current,host_);panel_=resize;
        panel_->setProperty("entrySize",current);resize->embedInEditor();
        auto source=PreviewImage::thumbnail(original_);
        auto *task=new ImageTask(resize);
        connect(task,&ImageTask::completed,resize,[this,resize](const QImage &image,const QString &error) {
            if(!error.isEmpty()){preview_->setToolTip(error);return;}
            preview_->setImage(QPixmap::fromImage(image));
            resize->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->setEnabled(true);
        });
        const auto refresh=[this,resize,source,task] {
            if(!ImageProcessor::validOutputSize(cv::Size(resize->targetSize().width(),resize->targetSize().height())))return;
            auto opt=draft_;const auto size=PreviewImage::boundedSize(resize->targetSize());
            opt.targetSize=cv::Size(size.width(),size.height());
            resize->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->setEnabled(false);
            task->submit(source,opt);
        };
        connect(resize,&ResizeDialog::targetSizeChanged,this,refresh);
        if(!reusePreview)refresh();
    }
    if(mode_!=Resize)DialogAppearance::embed(panel_);
    body_->addWidget(panel_);panel_->installEventFilter(this);
    panel_->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->installEventFilter(this);
    const auto changed=[this]{recordTimer_.start();updateButtons();};
    for(auto *spin:panel_->findChildren<QSpinBox *>())connect(spin,&QSpinBox::valueChanged,this,changed);
    for(auto *spin:panel_->findChildren<QDoubleSpinBox *>())connect(spin,&QDoubleSpinBox::valueChanged,this,changed);
    for(auto *slider:panel_->findChildren<QSlider *>())connect(slider,&QSlider::valueChanged,this,changed);
    for(auto *combo:panel_->findChildren<QComboBox *>())connect(combo,&QComboBox::currentIndexChanged,this,changed);
    for(auto *button:panel_->findChildren<QAbstractButton *>())connect(button,&QAbstractButton::clicked,this,changed);
    if(auto *selection=panel_->findChild<SelectionItem *>())connect(selection,&SelectionItem::selectionChanged,this,changed);
    if(auto *bg=qobject_cast<BackgroundDialog *>(panel_))connect(bg,&BackgroundDialog::optionsChanged,this,changed);
    modes_->button(mode_)->setChecked(true);panel_->show();updateButtons();
    host_->layout()->activate();setUpdatesEnabled(true);
}
