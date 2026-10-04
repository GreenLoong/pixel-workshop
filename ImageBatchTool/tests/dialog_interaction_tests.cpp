#include "mainwindow.h"
#include "tonedialog.h"
#include "previewlabel.h"
#include "geometrydialog.h"
#include "selectionitem.h"
#include "backgrounddialog.h"
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
#include <iostream>
#include <stdexcept>

namespace {
void require(bool pass, const char *message)
{
    if (!pass) throw std::runtime_error(message);
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
        QImage subject(80,80,QImage::Format_RGB888);subject.fill(QColor(20,35,210));
        for(int y=20;y<60;++y)for(int x=25;x<55;++x)subject.setPixelColor(x,y,QColor(220,70,40));
        ImageProcessor::Options bgOptions;bgOptions.background=ImageProcessor::BackgroundMode::Remove;bgOptions.feather=0;
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
        QImage transparent;
        for(auto *item:bgPreview->scene()->items())if(auto *p=qgraphicsitem_cast<QGraphicsPixmapItem *>(item))transparent=p->pixmap().toImage();
        require(transparent.pixelColor(0,0).alpha()==0 && transparent.pixelColor(40,40).alpha()==255,"Background preview alpha incorrect");
        bgDialog.grab().save("background-dialog.png");
        bgDialog.findChild<QComboBox *>("brushMode")->setCurrentIndex(2);
        ImageProcessor::BrushStroke bgStroke;bgStroke.foreground=false;bgStroke.radius=.06;bgStroke.points={{.5,.5}};
        static_cast<BrushPreview *>(bgPreview)->onStroke(bgStroke);
        bgDialog.findChild<QComboBox *>("brushMode")->setCurrentIndex(3);waitPreview();
        require(bgDialog.options().strokes.size()==1,"Brush correction not recorded");
        bgDialog.reject();
        require(bgOptions.strokes.empty(),"Cancelled dialog changed input options");
        std::cout<<"PASS: asynchronous background preview, alpha, mask initialization and cancel protection\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
