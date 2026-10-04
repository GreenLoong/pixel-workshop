#include "presentation/windows/mainwindow.h"
#include "ui_mainwindow.h"
#include "application/imageprocessing.h"
#include "presentation/pages/editorpage.h"
#include <QStackedWidget>
#include "presentation/dialogs/batchdialog.h"
#include "infrastructure/imagefiles.h"
#include "presentation/widgets/zoomcontrols.h"
#include <QFile>
#include <exception>
#include <stdexcept>
#include "presentation/widgets/previewlabel.h"

#include <QDebug>
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QToolButton>
#include <QFrame>
#include <QShortcut>
#include <QMouseEvent>
#include <QWindow>
#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QUndoStack>
#include <QUndoCommand>
#include <functional>

namespace
{

class ParameterCommand final : public QUndoCommand {
public:
    ParameterCommand(const ImageProcessor::Options &before,const ImageProcessor::Options &after,
                     std::function<void(const ImageProcessor::Options &)> apply)
        : before_(before),after_(after),apply_(std::move(apply)) {setText("图像编辑");}
    void undo() override {apply_(before_);}
    void redo() override {if(first_)first_=false;else apply_(after_);}
private:
    ImageProcessor::Options before_,after_;
    std::function<void(const ImageProcessor::Options &)> apply_;
    bool first_=true;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    pages_=new QStackedWidget(this);
    auto *home=takeCentralWidget();pages_->addWidget(home);setCentralWidget(pages_);
    editor_=new EditorPage(pages_);pages_->addWidget(editor_);
    setMinimumSize(1000,640);
    connect(editor_,&EditorPage::cancelled,this,&MainWindow::leaveEditor);
    connect(editor_,&EditorPage::accepted,this,[this] {
        if(applyProcessing(editor_->options()))leaveEditor();
    });
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    // 背景壳位于所有内容下方，阴影不施加到整棵控件树。
    windowFrame_ = new QFrame(this);
    windowFrame_->setObjectName("windowFrame");
    windowFrame_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *shadow = new QGraphicsDropShadowEffect(windowFrame_);
    shadow->setBlurRadius(22);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(15, 23, 42, 80));
    windowFrame_->setGraphicsEffect(shadow);
    windowFrame_->lower();
    updateWindowFrame();

    setupMenus();
    auto *brand=new QWidget(this);
    auto *brandLayout=new QHBoxLayout(brand);brandLayout->setContentsMargins(0,0,0,0);brandLayout->setSpacing(10);
    auto *appIcon=new QLabel(brand);appIcon->setObjectName("appIconLabel");
    appIcon->setFixedSize(44,44);appIcon->setPixmap(QIcon(":/app/icon.png").pixmap(44,44));
    const int titleIndex=ui->sidebarLayout->indexOf(ui->appTitleLabel);
    ui->sidebarLayout->removeWidget(ui->appTitleLabel);
    brandLayout->addWidget(appIcon);brandLayout->addWidget(ui->appTitleLabel,1);ui->sidebarLayout->insertWidget(titleIndex,brand);
    auto *editButton=new QToolButton(this);
    editButton->setObjectName("editImageButton");editButton->setText("编辑图片");
    connect(editButton,&QToolButton::clicked,this,&MainWindow::showEditor);
    editButton->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);editButton->setMinimumHeight(40);
    ui->adjustLayout->insertWidget(0,editButton);
    ui->editSectionLabel->setText("工作区");
    ui->shortcutHintLabel->setText("Ctrl+O 打开    Ctrl+S 保存\nCtrl+E 编辑    Ctrl+Z 撤销");
    auto *batchButton=new QPushButton("文件夹批量处理",this);batchButton->setObjectName("batchButton");
    ui->adjustLayout->addWidget(batchButton);connect(batchButton,&QPushButton::clicked,this,&MainWindow::showBatchDialog);
    applyTheme();
    setupZoomControls();
    setupWindowControls();
    installEventFilter(this);
    ui->centralwidget->installEventFilter(this);
    editor_->installEventFilter(this);
    statusBar()->installEventFilter(this);

    // 打开图片
    connect(ui->openImageButton, &QPushButton::clicked, this, &MainWindow::openImage);
    // 另存为
    connect(ui->saveImageButton, &QPushButton::clicked, this, &MainWindow::saveImage);
    // 信息卡片按最坏情况（4 行：尺寸/格式/文件/状态）锁定高度。
    // 只靠伸缩因子不够：卡片自身变高同样会把下方的“调整”区顶下去。
    {
        const QFontMetrics metrics(ui->imageInfoLabel->font());
        const int cardHeight = metrics.lineSpacing() * 4 + 44; // 4 行 + 内边距与边框余量

        ui->imageInfoLabel->setFixedHeight(cardHeight);
        ui->imageInfoLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    refreshImageUi();

    statusBar()->showMessage("打开一张图片开始处理");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupZoomControls()
{
    statusBar()->addPermanentWidget(new ZoomControls(ui->imageLabel,this));

    // 单独打开当前图片的全屏预览，主页的缩放和位置保持不变。
    auto *screenControls = new QWidget(this);
    screenControls->setObjectName("screenControls");
    auto *screenLayout = new QHBoxLayout(screenControls);
    screenLayout->setContentsMargins(3, 3, 3, 3);
    screenLayout->setSpacing(8);
    auto *divider = new QFrame(screenControls);
    divider->setFixedSize(1, 20);
    divider->setStyleSheet("background: #e3e6ec; border: none;");
    auto *fullScreen = new QToolButton(screenControls);
    fullScreen->setObjectName("fullScreenButton");
    fullScreen->setCursor(Qt::PointingHandCursor);
    fullScreen->setIcon(QIcon(":/indicators/fullscreen.svg"));
    fullScreen->setIconSize(QSize(18, 18));
    fullScreen->setFixedSize(30, 30);
    fullScreen->setToolTip("图片全屏预览（F11），Esc 返回");
    fullScreen->setAccessibleName("图片全屏预览");
    fullScreen->setEnabled(false);
    connect(ui->imageLabel, &PreviewLabel::imageAvailable, fullScreen, &QWidget::setEnabled);
    screenLayout->addWidget(divider);
    screenLayout->addWidget(fullScreen);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(screenControls);
    connect(fullScreen, &QToolButton::clicked, this, &MainWindow::showFullScreenPreview);
    auto *toggleShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(toggleShortcut, &QShortcut::activated, fullScreen, &QToolButton::click);
}

void MainWindow::setupWindowControls()
{
    auto *controls = new QWidget(menuBar());
    controls->setObjectName("windowControls");
    auto *layout = new QHBoxLayout(controls);
    layout->setContentsMargins(0, 3, 3, 3);
    layout->setSpacing(2);
    const auto makeButton = [controls, layout](const QString &name, const QString &icon,
                                                const QString &tip) {
        auto *button = new QToolButton(controls);
        button->setObjectName(name);
        button->setIcon(QIcon(icon));
        button->setIconSize(QSize(14,14));
        button->setFixedSize(36,30);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        layout->addWidget(button);
        return button;
    };
    auto *minimize = makeButton("minimizeWindowButton", ":/indicators/minimize.svg", "最小化");
    auto *maximize = makeButton("maximizeWindowButton", ":/indicators/maximize.svg", "最大化");
    auto *closeButton = makeButton("closeWindowButton", ":/indicators/close.svg", "关闭");
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximize, &QToolButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);
    menuBar()->setCornerWidget(controls, Qt::TopRightCorner);
    auto *caption=new QLabel("  Pixel Workshop",menuBar());caption->setObjectName("windowCaption");
    caption->setAttribute(Qt::WA_TransparentForMouseEvents);menuBar()->setCornerWidget(caption,Qt::TopLeftCorner);
    menuBar()->installEventFilter(this);
}

void MainWindow::showFullScreenPreview()
{
    if (currentImage.isNull())
        return;
    QPixmap displayed=currentImage;
    if(pages_->currentWidget()==editor_) {
        try{displayed=QPixmap::fromImage(ImageProcessing::processImage(originalImage.toImage(),editor_->options()));}
        catch(const std::exception &error){QMessageBox::warning(this,"无法预览",QString::fromUtf8(error.what()));return;}
    }
    QDialog dialog(this, Qt::Window | Qt::FramelessWindowHint);
    dialog.setObjectName("imageFullScreenDialog");
    dialog.setStyleSheet("QDialog#imageFullScreenDialog { background: black; }");
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *preview = new PreviewLabel(&dialog);
    preview->setObjectName("fullScreenPreview");
    preview->setStyleSheet("QGraphicsView { background: black; border: none; border-radius: 0; }");
    preview->setBackgroundBrush(Qt::black);
    preview->setImage(displayed);
    layout->addWidget(preview);
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), &dialog);
    connect(escape, &QShortcut::activated, &dialog, &QDialog::reject);
    auto *f11 = new QShortcut(QKeySequence(Qt::Key_F11), &dialog);
    connect(f11, &QShortcut::activated, &dialog, &QDialog::reject);
    dialog.showFullScreen();
    preview->setFocus();
    dialog.exec();
}

void MainWindow::updateWindowFrame()
{
    if (!windowFrame_)
        return;
    const bool maximized = isMaximized();
    const int margin = maximized ? 0 : 14;
    setContentsMargins(margin, margin, margin, margin);
    windowFrame_->setGeometry(contentsRect().adjusted(-1, -1, 1, 1));
    windowFrame_->setStyleSheet(maximized
        ? "background: #f2f3f5; border: none; border-radius: 0;"
        : "background: #f2f3f5; border: 1px solid #bdc5d2; border-radius: 13px;");
    windowFrame_->graphicsEffect()->setEnabled(!maximized);
    windowFrame_->lower();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateWindowFrame();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::WindowStateChange)
        return;
    updateWindowFrame();
    if (auto *button = findChild<QToolButton *>("maximizeWindowButton")) {
        button->setIcon(QIcon(isMaximized() ? ":/indicators/restore-window.svg"
                                            : ":/indicators/maximize.svg"));
        button->setToolTip(isMaximized() ? "还原" : "最大化");
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 进入内部控件或离开窗口时，清除父窗口光标，恢复子控件各自的光标。
    if ((event->type() == QEvent::Enter && watched != this)
        || (event->type() == QEvent::Leave && watched == this))
        unsetCursor();
    // 无标题栏时，利用窗口外沿留白调用系统的尺寸调整。
    if (watched == this && !isMaximized()
        && (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const QPoint point = mouse->position().toPoint();
        Qt::Edges edges;
        if (point.x() < contentsRect().left()) edges |= Qt::LeftEdge;
        if (point.x() > contentsRect().right()) edges |= Qt::RightEdge;
        if (point.y() < contentsRect().top()) edges |= Qt::TopEdge;
        if (point.y() > contentsRect().bottom()) edges |= Qt::BottomEdge;
        const bool horizontal = edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge);
        const bool vertical = edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge);
        if (horizontal && vertical)
            setCursor((edges.testFlag(Qt::LeftEdge) == edges.testFlag(Qt::TopEdge))
                      ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
        else
            setCursor(horizontal ? Qt::SizeHorCursor : vertical ? Qt::SizeVerCursor : Qt::ArrowCursor);
        if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton
            && edges && windowHandle() && windowHandle()->startSystemResize(edges))
            return true;
    }
    if ((event->type() == QEvent::MouseButtonPress
         || event->type() == QEvent::MouseButtonDblClick) && watched == menuBar()) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && !menuBar()->actionAt(mouse->position().toPoint())) {
            if (event->type() == QEvent::MouseButtonDblClick) {
                findChild<QToolButton *>("maximizeWindowButton")->click();
                return true;
            }
            if (windowHandle() && windowHandle()->startSystemMove())
                return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// 顶部菜单，主要提供键盘快捷键
void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");

    QAction *openAction = fileMenu->addAction("打开图片(&O)...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openImage);

    QAction *saveAction = fileMenu->addAction("另存为(&S)...");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveImage);
    auto *batch=fileMenu->addAction("文件夹批量处理…");
    batch->setShortcut(QKeySequence("Ctrl+Shift+B"));
    connect(batch,&QAction::triggered,this,&MainWindow::showBatchDialog);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction("退出(&X)");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);
    QMenu *edit = menuBar()->addMenu("编辑(&E)");
    edit->setObjectName("editMenu");
    history_=new QUndoStack(this);
    history_->setUndoLimit(50);
    auto *undo=history_->createUndoAction(this,"撤销");
    undo->setObjectName("undoAction");
    auto *redo=history_->createRedoAction(this,"重做");
    redo->setObjectName("redoAction");
    edit->addAction(undo);edit->addAction(redo);edit->addSeparator();
    auto *geometry = edit->addAction("裁剪与旋转…");
    geometry->setShortcut(QKeySequence("Ctrl+R"));
    connect(geometry,&QAction::triggered,this,&MainWindow::showGeometryDialog);
    auto *background=edit->addAction("背景编辑…");
    background->setShortcut(QKeySequence("Ctrl+B"));
    connect(background,&QAction::triggered,this,&MainWindow::showBackgroundDialog);
    auto *tone = edit->addAction("颜色与光线…");
    connect(tone,&QAction::triggered,this,&MainWindow::showToneDialog);
    auto *size = edit->addAction("调整大小…");
    connect(size,&QAction::triggered,this,&MainWindow::showResizeDialog);
    auto *gray = edit->addAction("灰度化");
    gray->setObjectName("grayscaleAction");gray->setCheckable(true);
    connect(gray,&QAction::triggered,this,[this](bool enabled){auto options=processingOptions_;options.grayscale=enabled;applyProcessing(options);});
    // 动作仍提供快捷键，标题栏不再重复展示文件和编辑菜单。
    for(auto *menu:{fileMenu,edit}) {
        menuBar()->removeAction(menu->menuAction());
        addActions(menu->actions());
    }
    auto *editShortcut=new QShortcut(QKeySequence("Ctrl+E"),this);
    connect(editShortcut,&QShortcut::activated,this,&MainWindow::showEditor);
    auto *undoShortcut=new QShortcut(QKeySequence::Undo,this);
    connect(undoShortcut,&QShortcut::activated,this,[this]{if(pages_->currentWidget()==editor_)editor_->undo();else history_->undo();});
    for(const auto &key:{QKeySequence(QKeySequence::Redo),QKeySequence("Ctrl+Shift+Z")}) {
        auto *shortcut=new QShortcut(key,this);
        connect(shortcut,&QShortcut::activated,this,[this]{if(pages_->currentWidget()==editor_)editor_->redo();else history_->redo();});
    }
}

// 应用整体风格
void MainWindow::applyTheme()
{
    QFile sheet(":/styles/mainwindow.qss");
    if(sheet.open(QIODevice::ReadOnly))setStyleSheet(QString::fromUtf8(sheet.readAll()));
}

// 打开图片
void MainWindow::openImage()
{
    QSettings settings;
    QString directory = settings.value("files/lastOpenDirectory").toString();
    if (!QDir(directory).exists() || directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty() || !QDir(directory).exists())
        directory = QDir::homePath();

    const QString filePath = QFileDialog::getOpenFileName(
        this, "选择图片", directory, "图片文件(*.png *.jpg *.jpeg *.bmp)");

    // 用户取消选择时，不改变当前状态
    if (filePath.isEmpty())
    {
        return;
    }

    qInfo() << "选中的图片:" << filePath;

    // 先读取到临时对象，确认成功后再更新界面
    QImageReader reader(filePath);reader.setAutoTransform(true);
    const QPixmap image=QPixmap::fromImage(reader.read());
    if (image.isNull())
    {
        QMessageBox::warning(this, "打开失败", "无法读取这张图片,检查文件是否损坏或者格式是否受支持");
        return;
    }

    // 只有图片读取成功才更新目录；取消或失败保留上次记录。
    settings.setValue("files/lastOpenDirectory", QFileInfo(filePath).absolutePath());

    originalImage = image;
    currentImage = image;
    currentFilePath = filePath;
    processingOptions_ = {};
    history_->clear();

    refreshImageUi();
    ui->imageLabel->fitToWindow();

    statusBar()->showMessage(QString("已打开：%1（%2 × %3 px）")
                                 .arg(QFileInfo(filePath).fileName())
                                 .arg(image.width())
                                 .arg(image.height()));
}

// 更新图片大小
void MainWindow::updatePreview()
{
    ui->imageLabel->setImage(currentImage);
}

// 刷新信息卡片
void MainWindow::updateImageInfo()
{
    if (currentImage.isNull())
    {
        ui->imageInfoLabel->setText("尚未打开图片");
        setWindowTitle("图像处理工具");
        return;
    }

    QStringList lines;
    lines << QString("尺寸  %1 × %2 px").arg(currentImage.width()).arg(currentImage.height());
    lines << QString("格式  %1").arg(currentFilePath.isEmpty()
                                         ? QStringLiteral("未命名")
                                         : QFileInfo(currentFilePath).suffix().toUpper());

    if (!currentFilePath.isEmpty())
        lines << QString("文件  %1").arg(QFileInfo(currentFilePath).fileName());

    QStringList adjustments;
    if (processingOptions_.grayscale) adjustments << "已灰度化";
    if (processingOptions_.rotation!=0) adjustments<<QString("旋转 %1°").arg(processingOptions_.rotation);
    if (processingOptions_.crop!=cv::Rect2d())adjustments<<"已裁剪";
    if (processingOptions_.flipHorizontal || processingOptions_.flipVertical)adjustments<<"已翻转";
    if (processingOptions_.background!=ImageProcessor::BackgroundMode::None)adjustments<<"已编辑背景";
    if (processingOptions_.hasColorAdjustments())adjustments<<"已调整颜色";
    if (processingOptions_.brightness != 0)
        adjustments << QString("亮度 %1").arg(processingOptions_.brightness);
    if (processingOptions_.contrast != 1.0)
        adjustments << QString("对比度 %1").arg(processingOptions_.contrast, 0, 'f', 2);
    if (!adjustments.isEmpty()) {
        const QString state = "状态  " + adjustments.join(" · ");
        ui->imageInfoLabel->setToolTip(state);
        lines << QFontMetrics(ui->imageInfoLabel->font()).elidedText(
            state, Qt::ElideRight, qMax(160, ui->imageInfoLabel->width() - 24));
    } else {
        ui->imageInfoLabel->setToolTip(QString());
    }

    ui->imageInfoLabel->setText(lines.join('\n'));

    const QString title = currentFilePath.isEmpty()
                              ? QStringLiteral("图像处理工具")
                              : QString("%1 - 图像处理工具").arg(QFileInfo(currentFilePath).fileName());

    setWindowTitle(title);
}

// 根据当前状态启用或禁用按钮
void MainWindow::updateActionState()
{
    const bool hasImage = !currentImage.isNull();

    ui->saveImageButton->setEnabled(hasImage);
    if(auto *edit=findChild<QMenu *>("editMenu"))edit->setEnabled(hasImage);
    findChild<QToolButton *>("editImageButton")->setEnabled(hasImage);
    findChild<QAction *>("grayscaleAction")->setChecked(processingOptions_.grayscale);
}

// 灰度化
void MainWindow::converToGrayscale()
{
    if (originalImage.isNull())
    {
        QMessageBox::information(this, "提示", "请先打开一张图片");
        return;
    }

    auto options = processingOptions_;
    options.grayscale = true;
    applyProcessing(options);
}

// 恢复原图
void MainWindow::restoreOriginal()
{
    if (originalImage.isNull())
        return;

    applyProcessing({});

    statusBar()->showMessage("已恢复为原图");
}

// 另存为
void MainWindow::saveImage()
{
    if (currentImage.isNull())
    {
        QMessageBox::information(this, "提示", "请先打开一张图片");
        return;
    }

    // 不会弹出是否覆盖已有文件对话框
    QString outputPath = QFileDialog::getSaveFileName(this, "保存处理结果", "result.png", "PNG 图片(*.png)", nullptr, QFileDialog::DontConfirmOverwrite);

    // 取消保存
    if (outputPath.isEmpty())
        return;

    try {
        outputPath = ImageFiles::saveUniquePng(currentImage.toImage(), outputPath);
    } catch(const std::exception &error) {
        QMessageBox::warning(this,"保存失败",QString::fromUtf8(error.what()));
        return;
    }
    statusBar()->showMessage(QString("已保存：%1").arg(outputPath), 5000);

    QMessageBox::information(this, "保存成功", "图片保存到: \n" + outputPath);
}

void MainWindow::showResizeDialog()
{
    enterEditor(EditorPage::Resize);
}

void MainWindow::showToneDialog()
{
    enterEditor(EditorPage::Tone);
}

void MainWindow::showGeometryDialog()
{
    enterEditor(EditorPage::Crop);
}

void MainWindow::showBackgroundDialog()
{
    enterEditor(EditorPage::Background);
}
void MainWindow::showEditor(){enterEditor(EditorPage::Crop);}
void MainWindow::enterEditor(int mode)
{
    if(originalImage.isNull())return;
    if(pages_->currentWidget()==editor_){editor_->selectMode(static_cast<EditorPage::Mode>(mode));return;}
    editor_->begin(originalImage.toImage(),processingOptions_,QFileInfo(currentFilePath).fileName(),static_cast<EditorPage::Mode>(mode));
    pages_->setCurrentWidget(editor_);statusBar()->hide();
    for(auto *action:actions())action->setEnabled(false);
    editor_->setFocus();
}
void MainWindow::leaveEditor()
{
    pages_->setCurrentIndex(0);statusBar()->show();
    editor_->end();
    for(auto *action:actions())action->setEnabled(true);
    findChild<QAction *>("undoAction")->setEnabled(history_->canUndo());
    findChild<QAction *>("redoAction")->setEnabled(history_->canRedo());
    updateActionState();
}

void MainWindow::showBatchDialog()
{
    BatchDialog dialog(processingOptions_,this);
    dialog.exec();
}

void MainWindow::refreshImageUi()
{
    updateActionState();
    updateImageInfo();
    updatePreview();
}

bool MainWindow::applyProcessing(const ImageProcessor::Options &options, bool recordHistory)
{
    if (originalImage.isNull())
        return false;
    try {
        const auto before=processingOptions_;
        QPixmap processed = options.isIdentity(cv::Size(originalImage.width(),originalImage.height()))
            ?originalImage:QPixmap::fromImage(ImageProcessing::processImage(originalImage.toImage(), options));
        if (processed.isNull())
            throw std::runtime_error("Unable to create the output pixmap");
        // 结果成功后一起提交图片和参数，失败时保留上一份有效状态。
        currentImage = processed;
        processingOptions_ = options;
        if(recordHistory)
            history_->push(new ParameterCommand(before,options,[this](const auto &state) {
                applyProcessing(state,false);
            }));
        refreshImageUi();
        statusBar()->showMessage(QString("处理完成：%1 × %2 px")
                                 .arg(processed.width()).arg(processed.height()));
        return true;
    } catch (const std::exception &error) {
        QMessageBox::warning(this, "处理失败", QString::fromUtf8(error.what()));
        return false;
    }
}
