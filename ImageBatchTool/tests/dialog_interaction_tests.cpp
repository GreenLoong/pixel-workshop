#include "mainwindow.h"
#include "tonedialog.h"
#include "previewlabel.h"
#include "geometrydialog.h"
#include "selectionitem.h"
#include "backgrounddialog.h"
#include "batchdialog.h"
#include <QFontDatabase>
#include <QLineEdit>
#include <QLabel>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QAction>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QImageWriter>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QScrollBar>
#include <QStyleFactory>
#include <QSlider>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCheckBox>
#include <QRadioButton>
#include "editorpage.h"
#include <QStackedWidget>
#include <QColorDialog>
#include <QElapsedTimer>
#include <QThread>
#include <QPointer>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool pass, const char *message)
{
    if (!pass) throw std::runtime_error(message);
}
template<class Predicate> void waitFor(Predicate predicate)
{
    QElapsedTimer elapsed;elapsed.start();
    while(!predicate() && elapsed.elapsed()<5000){QApplication::processEvents();QThread::msleep(1);}
    require(predicate(),"Timed out waiting for UI state");
    QApplication::processEvents();
}
void pointer(QWidget *widget,QEvent::Type type,QPoint point)
{
    QMouseEvent e(type,point,widget->mapToGlobal(point),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,
        type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(widget,&e);
}
QImage previewImage(ToneDialog &dialog)
{
    auto *preview = dialog.findChild<PreviewLabel *>("tonePreview");
    for (auto *item : preview->scene()->items())
        if (auto *pixmap = qgraphicsitem_cast<QGraphicsPixmapItem *>(item))
            return pixmap->pixmap().toImage();
    throw std::runtime_error("Missing preview image");
}
void mouse(QSlider *slider, QEvent::Type type, double fraction)
{
    const QPointF pos(slider->width() * fraction, slider->height() / 2.0);
    const bool moving = type == QEvent::MouseMove;
    QMouseEvent event(type, pos, slider->mapToGlobal(pos.toPoint()),
        moving ? Qt::NoButton : Qt::LeftButton,
        type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(slider, &event);
}

void checkRoundedPreview(PreviewLabel *preview, const QColor &imageColor)
{
    auto *viewport = preview->viewport();
    const QRegion mask = viewport->mask();
    require(!mask.isEmpty(), "Rounded preview has no viewport clipping");
    const QImage rendered = preview->window()->grab().toImage();
    const double dpr = rendered.devicePixelRatio();
    const auto colorAt = [&](const QPoint &point) {
        const QPoint local = viewport->mapTo(preview->window(), point);
        return rendered.pixelColor(qRound(local.x() * dpr), qRound(local.y() * dpr));
    };
    const QRect rect = viewport->rect();
    for (const QPoint point : {rect.topLeft(), rect.topRight(), rect.bottomLeft(), rect.bottomRight()}) {
        require(!mask.contains(point), "Image viewport still has square corners");
        require(colorAt(point) != imageColor, "Zoomed image painted over a rounded corner");
    }
    require(colorAt(rect.center()) == imageColor, "Clipping removed the image center");
}

void checkWheelInBlankArea(PreviewLabel *preview)
{
    preview->setZoomPercent(10);
    QApplication::processEvents();
    const QPoint blank(8, preview->viewport()->height() / 2);
    require(!preview->sceneRect().contains(preview->mapToScene(blank)),
            "Wheel fixture point is not outside the image");
    const auto sendWheel = [&](int delta) {
        QWheelEvent wheel(blank, preview->viewport()->mapToGlobal(blank),
            QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        wheel.ignore();
        QApplication::sendEvent(preview->viewport(), &wheel);
        require(wheel.isAccepted(), "Wheel over preview whitespace was ignored");
    };
    sendWheel(120);
    require(preview->zoomPercent() == 12, "Cannot zoom in over preview whitespace");
    sendWheel(-120);
    require(preview->zoomPercent() == 10, "Cannot zoom out over preview whitespace");
}
}

int main(int argc, char **argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc"); // 离屏平台的中文截图。
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    std::cout << std::unitbuf;
    app.setOrganizationName("PixelWorkshopTests");
    app.setApplicationName("DialogInteraction");
    QTemporaryDir temp(QDir::current().filePath("dialog-tests-XXXXXX"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    try {
        if (!temp.isValid())
            throw std::runtime_error(temp.errorString().toStdString());
        QImage original(2400, 1600, QImage::Format_RGB888);
        original.fill(Qt::red);
        ImageProcessor::Options options;
        options.grayscale = true;
        options.targetSize = cv::Size(960, 640);
        ToneDialog dialog(original, options);
        dialog.show();
        app.processEvents();
        require(dialog.windowFlags().testFlag(Qt::FramelessWindowHint), "Native title bar remains");
        require(dialog.graphicsEffect(), "Missing dialog shadow");
        auto *brightness = dialog.findChild<QSlider *>("brightnessSlider");
        auto *contrast = dialog.findChild<QSlider *>("contrastSlider");
        require(previewImage(dialog).pixelColor(0, 0).red() == 76, "Initial grayscale preview");
        auto *tonePreview = dialog.findChild<PreviewLabel *>("tonePreview");
        tonePreview->setZoomPercent(800);
        app.processEvents();
        checkRoundedPreview(tonePreview, QColor(76, 76, 76));
        checkWheelInBlankArea(tonePreview);
        for (int value : {20, 40, -10}) {
            brightness->setValue(value);
            require(previewImage(dialog).pixelColor(0, 0).red() == 76 + value,
                    "Preview did not update synchronously");
        }
        brightness->setValue(0);
        mouse(brightness, QEvent::MouseButtonPress, 0.75);
        require(brightness->value() >= 45 && brightness->value() <= 55,
                "Track click did not jump to the mouse");
        mouse(brightness, QEvent::MouseMove, 0.85);
        require(brightness->isSliderDown() && brightness->value() > 60,
                "Drag after track click did not track the pointer");
        require(previewImage(dialog).pixelColor(0, 0).red() == 76 + brightness->value(),
                "Preview waited for release during a drag");
        mouse(brightness, QEvent::MouseButtonRelease, 0.85);
        brightness->setValue(0);
        mouse(contrast, QEvent::MouseButtonPress, 0.75);
        require(contrast->value() >= 157 && contrast->value() <= 167,
                "Contrast track click did not jump");
        mouse(contrast, QEvent::MouseButtonRelease, 0.75);
        const auto edited = dialog.options();
        require(edited.grayscale && edited.targetSize == options.targetSize,
                "Tone edit discarded grayscale or size");
        require(previewImage(dialog).pixelColor(0, 0).red() == qRound(76 * edited.contrast),
                "Contrast preview did not update synchronously");
        dialog.findChild<QPushButton *>("resetToneButton")->click();
        require(dialog.options().brightness == 0 && dialog.options().contrast == 1.0
                && brightness->value() == 0 && contrast->value() == 100,
                "Reset did not synchronize controls");
        dialog.grab().save(temp.path() + "/tone.png");
        dialog.findChild<QToolButton *>("closeButton")->click();
        require(dialog.result() == QDialog::Rejected, "Close did not cancel");
        std::cout << "PASS: frameless style, instant preview, track jump, pressed drag, reset and cancel\n";

        const QString first = temp.path() + "/first";
        GeometryDialog geometry(original, options);
        geometry.show(); app.processEvents();
        require(geometry.options().targetSize==options.targetSize,"Unchanged geometry reset size");
        geometry.findChild<QDoubleSpinBox *>("rotationSpinBox")->setValue(90);
        auto *selection=geometry.findChild<SelectionItem *>();
        const auto bounds=selection->boundingRect().adjusted(8,8,-8,-8);
        selection->setSelection(QRectF(bounds.width()*0.25,bounds.height()*0.25,
                                      bounds.width()*0.5,bounds.height()*0.5));
        require(geometry.options().rotation==90 && geometry.options().targetSize==cv::Size()
            && std::abs(geometry.options().crop.width-0.5)<1e-9,
            "Crop/rotation editor parameters");
        const auto crop = selection->selection();
        for (const auto entry : {std::pair<QPointF,Qt::CursorShape>{crop.center(),Qt::SizeAllCursor},
             {QPointF(crop.left(),crop.center().y()),Qt::SizeHorCursor},
             {QPointF(crop.center().x(),crop.top()),Qt::SizeVerCursor},
             {crop.topLeft(),Qt::SizeFDiagCursor}, {crop.topRight(),Qt::SizeBDiagCursor}}) {
            QGraphicsSceneHoverEvent hover(QEvent::GraphicsSceneHoverMove);
            hover.setPos(entry.first); selection->scene()->sendEvent(selection,&hover);
            require(selection->cursor().shape()==entry.second,"Crop edge/corner cursor incorrect");
        }
        require(geometry.findChild<PreviewLabel *>("geometryPreview")->dragMode()==QGraphicsView::NoDrag,
                "Crop selection is competing with hand panning");
        geometry.reject();
        std::cout<<"PASS: geometry editor preserves unchanged size and maps crop coordinates\n";
        const QString second = temp.path() + "/second";
        QDir().mkpath(first);
        QDir().mkpath(second);
        const QString file = second + "/sample.png";
        QImageWriter writer(file, "png");
        if (!writer.write(original))
            throw std::runtime_error(writer.errorString().toStdString());
        QSettings().setValue("files/lastOpenDirectory", first);
        MainWindow window;
        window.show();
        bool handled = false;
        QTimer::singleShot(0, [&] {
            auto *picker = window.findChild<QFileDialog *>();
            if (!picker) return;
            handled = picker->directory().absolutePath() == first;
            picker->selectFile(file);
            QMetaObject::invokeMethod(picker, "accept");
        });
        QMetaObject::invokeMethod(&window, "openImage");
        require(handled, "Picker ignored the stored directory");
        require(QSettings().value("files/lastOpenDirectory").toString() == second,
                "Selected directory was not persisted");
        app.processEvents();
        auto *zoom = window.findChild<QSlider *>("zoomSlider");
        auto *mainPreview = window.findChild<PreviewLabel *>("imageLabel");
        auto *percent = window.findChild<QComboBox *>("zoomPercent");
        zoom->setValue(100);
        mouse(zoom, QEvent::MouseButtonPress, 0.75);
        require(zoom->value() >= 550 && zoom->value() <= 650,
                "Main zoom track click still uses step increments");
        require(mainPreview->zoomPercent() == zoom->value()
                && percent->currentText() == QString::number(zoom->value()) + "%",
                "Click jump did not synchronize preview and percent text");
        mouse(zoom, QEvent::MouseMove, 0.85);
        require(zoom->isSliderDown() && mainPreview->zoomPercent() > 650,
                "Zoom drag after click did not follow the pointer");
        mouse(zoom, QEvent::MouseButtonRelease, 0.85);
        mainPreview->setZoomPercent(800);
        app.processEvents();
        checkRoundedPreview(mainPreview, QColor(Qt::red));
        mainPreview->horizontalScrollBar()->setValue(mainPreview->horizontalScrollBar()->maximum());
        mainPreview->verticalScrollBar()->setValue(mainPreview->verticalScrollBar()->maximum());
        window.resize(1050, 720);
        app.processEvents();
        checkRoundedPreview(mainPreview, QColor(Qt::red));
        require(mainPreview->zoomPercent() == 800, "Mask changed the image zoom");
        window.grab().save("preview-rounded.png");
        mainPreview->setCornerRadius(0);
        require(mainPreview->viewport()->mask().isEmpty(), "Square fullscreen mode kept the clipping mask");
        mainPreview->setCornerRadius(12);
        checkRoundedPreview(mainPreview, QColor(Qt::red));
        checkWheelInBlankArea(mainPreview);
        require(zoom->value() == 10 && percent->currentText() == "10%",
                "Wheel over whitespace did not synchronize zoom controls");
        PreviewLabel fullscreenPreview;
        fullscreenPreview.resize(800, 600);
        fullscreenPreview.setImage(QPixmap::fromImage(original));
        fullscreenPreview.showFullScreen();
        app.processEvents();
        checkWheelInBlankArea(&fullscreenPreview);
        fullscreenPreview.close();
        PreviewLabel emptyPreview;
        emptyPreview.resize(640, 480);
        emptyPreview.show();
        app.processEvents();
        const QPoint emptyCenter = emptyPreview.viewport()->rect().center();
        QWheelEvent emptyWheel(emptyCenter, emptyPreview.viewport()->mapToGlobal(emptyCenter),
            QPoint(), QPoint(0,120), Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QApplication::sendEvent(emptyPreview.viewport(), &emptyWheel);
        require(!emptyWheel.isAccepted() && emptyPreview.zoomPercent() == 100,
                "Empty preview reacted to wheel events");
        emptyPreview.close();
        std::cout << "PASS: wheel zoom over whitespace in main, tone and fullscreen previews; empty view ignored\n";
        std::cout << "PASS: main zoom click/drag, rounded clipping at 800%, scroll, resize and square mode\n";
        handled = false;
        QTimer::singleShot(0, [&] {
            auto *picker = window.findChild<QFileDialog *>();
            if (!picker) return;
            handled = picker->directory().absolutePath() == second;
            picker->reject();
        });
        QMetaObject::invokeMethod(&window, "openImage");
        require(handled && QSettings().value("files/lastOpenDirectory").toString() == second,
                "Cancel lost the last successful directory");
        std::cout << "PASS: persisted open directory, repeated opening and cancel\n";
        auto *undo=window.findChild<QAction *>("undoAction");
        auto *redo=window.findChild<QAction *>("redoAction");
        require(!undo->isEnabled(),"New image kept old history");
        QMetaObject::invokeMethod(&window,"converToGrayscale");
        require(undo->isEnabled() && previewImage(dialog).isNull()==false,"No undo after edit");
        undo->trigger();
        auto imageFromMain=[&] {
            for(auto *item:mainPreview->scene()->items())
                if(auto *p=qgraphicsitem_cast<QGraphicsPixmapItem *>(item))return p->pixmap().toImage();
            return QImage();
        };
        require(imageFromMain().pixelColor(0,0)==QColor(Qt::red),"Undo did not restore color");
        redo->trigger();
        require(imageFromMain().pixelColor(0,0).red()==76,"Redo did not restore grayscale");
        QMetaObject::invokeMethod(&window,"restoreOriginal");
        undo->trigger();
        require(imageFromMain().pixelColor(0,0).red()==76,"Restore cannot be undone");
        std::cout<<"PASS: edit undo/redo and undo original restoration\n";
        QMetaObject::invokeMethod(&window,"showToneDialog");app.processEvents();
        auto *editorPage=window.findChild<EditorPage *>();
        require(editorPage->isVisible() && !editorPage->findChild<ToneDialog *>()->isWindow(),"Editing still opens a separate window");
        QPointer<PreviewLabel> canvas=editorPage->findChild<PreviewLabel *>("editorPreview");
        require(canvas,"Missing shared editor canvas");
        canvas->setZoomPercent(200);
        canvas->horizontalScrollBar()->setValue(canvas->horizontalScrollBar()->maximum()/3);
        canvas->verticalScrollBar()->setValue(canvas->verticalScrollBar()->maximum()/4);
        const QRect canvasRect(canvas->mapTo(editorPage,QPoint()),canvas->size());
        const QPointF centerBefore=canvas->mapToScene(canvas->viewport()->rect().center());
        int imageUpdates=0;
        const auto updateConnection=QObject::connect(canvas,&PreviewLabel::imageAvailable,editorPage,[&](bool){++imageUpdates;});
        for(auto mode:{EditorPage::Resize,EditorPage::Crop,EditorPage::Background,EditorPage::Tone}) {
            imageUpdates=0;editorPage->selectMode(mode);app.processEvents();
            waitFor([&]{return editorPage->findChild<QPushButton *>("confirmButton",Qt::FindDirectChildrenOnly)->isEnabled();});
            require(canvas && editorPage->findChild<PreviewLabel *>("editorPreview")==canvas,"Mode switch recreated the preview");
            require(QRect(canvas->mapTo(editorPage,QPoint()),canvas->size())==canvasRect,"Preview bounds moved between modes");
            require(canvas->zoomPercent()==200,"Mode switch reset manual zoom");
            const auto centerAfter=canvas->mapToScene(canvas->viewport()->rect().center());
            require(QLineF(centerBefore,centerAfter).length()<2,"Mode switch reset image panning");
            if(mode==EditorPage::Resize)require(imageUpdates==0,"Size mode needlessly regenerated the existing result");
            require(editorPage->findChildren<PreviewLabel *>().size()==1,"Multiple editor canvases remain alive");
        }
        QObject::disconnect(updateConnection);
        std::cout<<"PASS: fixed preview bounds, persistent canvas, zoom/pan and cached result on entering resize\n";
        editorPage->findChild<QSlider *>("brightnessSlider")->setValue(25);
        editorPage->selectMode(EditorPage::Resize);app.processEvents();
        require(editorPage->options().brightness==25,"Changing mode discarded tone draft");
        editorPage->findChild<QSpinBox *>("widthSpinBox")->setValue(300);
        require(editorPage->options().targetSize==cv::Size(300,200),"Embedded resize aspect ratio failed");
        editorPage->grab().save("editor-size.png");
        editorPage->selectMode(EditorPage::Crop);app.processEvents();
        require(editorPage->options().targetSize==cv::Size(300,200),"Entering crop silently reset target size");
        editorPage->undo();
        require(editorPage->options().brightness==25 && editorPage->options().targetSize==cv::Size(),"Draft undo lost unrelated tone change");
        editorPage->redo();require(editorPage->options().targetSize==cv::Size(300,200),"Draft redo failed");
        editorPage->findChild<QPushButton *>("cancelEditButton")->click();app.processEvents();
        require(!editorPage->isVisible() && imageFromMain().pixelColor(0,0).red()==76 && imageFromMain().size()==original.size(),
                "Cancel committed editor draft to main image");
        QMetaObject::invokeMethod(&window,"showToneDialog");
        editorPage->findChild<QSlider *>("brightnessSlider")->setValue(15);
        editorPage->findChild<QPushButton *>("confirmButton")->click();app.processEvents();
        require(!editorPage->isVisible() && imageFromMain().pixelColor(0,0).red()==91,"Finish did not apply editor draft");
        undo->trigger();require(imageFromMain().pixelColor(0,0).red()==76,"Whole editing session cannot be undone");
        window.grab().save("home-simplified.png");
        std::cout<<"PASS: in-window editor, combined mode draft, resize, local undo/redo, cancel and atomic finish\n";
        QImage subject(80,80,QImage::Format_RGB888);subject.fill(QColor(20,35,210));
        for(int y=20;y<60;++y)for(int x=25;x<55;++x)subject.setPixelColor(x,y,QColor(220,70,40));
        ImageProcessor::Options bgOptions;bgOptions.background=ImageProcessor::BackgroundMode::Remove;bgOptions.feather=0;
        bgOptions.segmentation=ImageProcessor::SegmentationMethod::Region;
        BackgroundDialog bgDialog(subject,bgOptions);bgDialog.show();
        require(bgDialog.windowFlags().testFlag(Qt::FramelessWindowHint),"Background dialog kept native title");
        require(bgDialog.options().foregroundRect.width<1,"Foreground region overwritten by initialization");
        auto waitPreview=[&] {
            QEventLoop loop;QTimer poll,timeout;
            auto *ok=bgDialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(ok->isEnabled())loop.quit();});
            QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
            poll.start(10);timeout.setSingleShot(true);timeout.start(5000);loop.exec();
            require(ok->isEnabled(),"Background preview failed or stopped updating");
        };
        waitPreview();
        auto *bgPreview=bgDialog.findChild<PreviewLabel *>("backgroundPreview");
        auto *region=bgDialog.findChild<SelectionItem *>();
        const auto initialRegion=region->selection();
        bgDialog.findChild<QPushButton *>("rangeSelectionButton")->setChecked(true);
        region->setSelection(bgPreview->sceneRect());waitPreview();
        require(bgDialog.findChild<QLabel *>("backgroundMessage")->text().contains("没有背景标记"),
                "Full selection should give actionable guidance instead of OpenCV error");
        region->setSelection(initialRegion);waitPreview();
        require(bgDialog.findChild<QLabel *>("backgroundMessage")->text().startsWith("区域分割已更新"),
                "Region segmentation did not recover after shrinking full selection");
        bgDialog.findChild<QPushButton *>("rangeSelectionButton")->setChecked(false);waitPreview();
        QImage transparent;
        for(auto *item:bgPreview->scene()->items())if(auto *p=qgraphicsitem_cast<QGraphicsPixmapItem *>(item))transparent=p->pixmap().toImage();
        require(transparent.pixelColor(0,0).alpha()==0 && transparent.pixelColor(40,40).alpha()==255,"Background preview alpha incorrect");
        bgDialog.grab().save("background-dialog.png");
        bgDialog.findChild<QCheckBox *>("brushEnabled")->setChecked(true);
        bgDialog.findChild<QRadioButton *>("removeBrush")->setChecked(true);
        waitPreview();
        auto *brushPreview=static_cast<BrushPreview *>(bgPreview);
        require(brushPreview->painting && brushPreview->dragMode()==QGraphicsView::NoDrag,"Refresh disabled brush mode");
        const auto center=brushPreview->mapFromScene(QPointF(40,40));
        pointer(brushPreview->viewport(),QEvent::MouseMove,center);app.processEvents();
        const auto smallBrush=brushPreview->viewport()->grab().toImage();
        bgDialog.findChild<QSlider *>("brushRadiusSlider")->setValue(60);app.processEvents();
        require(smallBrush!=brushPreview->viewport()->grab().toImage(),"Brush diameter is not shown at pointer");
        pointer(brushPreview->viewport(),QEvent::MouseButtonPress,center);
        pointer(brushPreview->viewport(),QEvent::MouseMove,brushPreview->mapFromScene(QPointF(45,40)));
        pointer(brushPreview->viewport(),QEvent::MouseButtonRelease,brushPreview->mapFromScene(QPointF(45,40)));
        bgDialog.findChild<QCheckBox *>("brushEnabled")->setChecked(false);waitPreview();
        require(bgDialog.options().strokes.size()==1,"Brush correction not recorded");
        for(auto *item:bgPreview->scene()->items())if(auto *p=qgraphicsitem_cast<QGraphicsPixmapItem *>(item))
            require(p->pixmap().toImage().pixelColor(40,40).alpha()==0,"Painted region did not change the actual mask");
        require(bgDialog.options().strokes[0].points.size()>=2 && !bgDialog.options().strokes[0].foreground,"Real mouse drag did not paint a background stroke");
        require(bgDialog.findChild<QWidget *>("blurPanel")->isHidden(),"Blur controls visible in remove mode");
        bgDialog.findChild<QComboBox *>("backgroundMode")->setCurrentIndex(1);waitPreview();
        require(!bgDialog.findChild<QWidget *>("blurPanel")->isHidden(),"Blur controls missing in blur mode");
        bgDialog.findChild<QComboBox *>("backgroundMode")->setCurrentIndex(3);waitPreview();
        bool colorChecked=false;
        QTimer::singleShot(0,[&] {
            auto *picker=bgDialog.findChild<QColorDialog *>();if(!picker)return;
            picker->show();app.processEvents();picker->grab().save("color-picker.png");
            colorChecked=picker->grab().toImage().pixelColor(5,5).lightness()>180;
            picker->setCurrentColor(QColor(240,80,20));picker->accept();
        });
        bgDialog.findChild<QPushButton *>("backgroundColorButton")->click();waitPreview();
        require(colorChecked && bgDialog.options().backgroundColor[0]==240,"Color picker background or selected color invalid");
        bgDialog.reject();
        require(bgOptions.strokes.empty(),"Cancelled dialog changed input options");
        std::cout<<"PASS: asynchronous background preview, alpha, mask initialization and cancel protection\n";
        BatchDialog batchDialog(options);batchDialog.show();app.processEvents();batchDialog.grab().save("batch-dialog.png");
        require(batchDialog.windowFlags().testFlag(Qt::FramelessWindowHint),"Batch dialog kept native title");
        batchDialog.findChild<QLineEdit *>("batchInput")->setText(second);
        batchDialog.findChild<QLineEdit *>("batchOutput")->setText(temp.path()+"/batch-output");
        batchDialog.findChild<QPushButton *>("confirmButton")->click();
        require(!batchDialog.findChild<QLineEdit *>("batchInput")->isEnabled(),"Batch parameters editable during task");
        auto *job=batchDialog.findChild<BatchJob *>();require(job && job->isRunning(),"Batch UI did not start worker");
        QEventLoop batchLoop;QTimer batchTimeout;batchTimeout.setSingleShot(true);
        QObject::connect(&batchTimeout,&QTimer::timeout,&batchLoop,&QEventLoop::quit);
        QObject::connect(job,&BatchJob::finished,&batchLoop,&QEventLoop::quit);
        batchDialog.reject();batchTimeout.start(5000);batchLoop.exec();
        require(!job->isRunning() && batchDialog.result()==QDialog::Rejected,"Closing active batch did not safely cancel");
        std::cout<<"PASS: batch UI starts background task, freezes parameters and safely closes during cancellation\n";
        if(argc>1) {
            const QImage portrait(QString::fromLocal8Bit(argv[1]));require(!portrait.isNull(),"Portrait fixture unreadable");
            ImageProcessor::Options humanOptions;humanOptions.background=ImageProcessor::BackgroundMode::Remove;
            BackgroundDialog humanDialog(portrait,humanOptions);humanDialog.show();
            auto *engine=humanDialog.findChild<QComboBox *>("segmentationMethod");
            require(engine->currentIndex()==0,"Portrait model is not the default");
            auto *range=humanDialog.findChild<QPushButton *>("rangeSelectionButton");
            require(range->isHidden(),"Human mode allowed the old bounding-box background rule");
            QEventLoop humanLoop;QTimer poll,timeout;timeout.setSingleShot(true);
            auto *ok=humanDialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            QObject::connect(&poll,&QTimer::timeout,&humanLoop,[&]{if(ok->isEnabled())humanLoop.quit();});
            QObject::connect(&timeout,&QTimer::timeout,&humanLoop,&QEventLoop::quit);
            poll.start(10);timeout.start(5000);humanLoop.exec();require(ok->isEnabled(),"Human dialog preview failed");
            humanDialog.grab().save("human-background-dialog.png");
            engine->setCurrentIndex(1);
            require(!range->isHidden(),"Region mode did not restore bounding-box control");
            humanDialog.reject();
            require(humanOptions.segmentation==ImageProcessor::SegmentationMethod::Human,"Cancelled dialog changed original method");
            std::cout<<"PASS: default portrait UI, independent region mode and cancellation\n";
            EditorPage portraitEditor;portraitEditor.resize(1180,790);
            portraitEditor.begin(portrait,humanOptions,"人像样例.jpg");portraitEditor.show();app.processEvents();
            portraitEditor.grab().save("editor-crop.png");
            portraitEditor.selectMode(EditorPage::Tone);app.processEvents();portraitEditor.grab().save("editor-tone.png");
            portraitEditor.selectMode(EditorPage::Background);app.processEvents();
            auto ready=[&]{return portraitEditor.findChild<QPushButton *>("confirmButton",Qt::FindDirectChildrenOnly)->isEnabled();};
            waitFor(ready);
            portraitEditor.findChild<QCheckBox *>("brushEnabled")->setChecked(true);waitFor(ready);
            auto *view=portraitEditor.findChild<PreviewLabel *>("editorPreview");
            pointer(view->viewport(),QEvent::MouseMove,view->mapFromScene(view->sceneRect().center()));
            app.processEvents();portraitEditor.grab().save("editor-background.png");
            require(!portraitEditor.findChild<QWidget *>("brushPanel")->isHidden(),"Brush panel not shown in editing page");
            portraitEditor.end();portraitEditor.close();
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
