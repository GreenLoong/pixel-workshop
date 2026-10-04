#include "presentation/windows/mainwindow.h"
#include "presentation/pages/editorpage.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QAction>
#include <QSlider>
#include <QPushButton>
#include <QStyleFactory>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
template<class Predicate> void waitFor(Predicate predicate) {
    QElapsedTimer timer;timer.start();
    while(!predicate() && timer.elapsed()<5000){QApplication::processEvents();QThread::msleep(1);}
    require(predicate(),"UI timed out");
}
void open(MainWindow &window,const QString &path,int choice=QMessageBox::Cancel) {
    QTimer::singleShot(0,&window,[&window,path,choice] {
        auto *picker=window.findChild<QFileDialog *>();require(picker,"Missing open picker");
        picker->selectFile(path);QMetaObject::invokeMethod(picker,"accept");
        QTimer::singleShot(0,&window,[&window,choice] {
            if(auto *prompt=window.findChild<QMessageBox *>("unsavedChangesDialog"))prompt->done(choice);
        });
    });
    QMetaObject::invokeMethod(&window,"openImage");
}
void answerClose(MainWindow &window,int choice) {
    QTimer::singleShot(0,&window,[&window,choice] {
        auto *prompt=window.findChild<QMessageBox *>("unsavedChangesDialog");require(prompt,"Missing unsaved prompt");
        prompt->done(choice);
    });
    window.close();
}
}
int main(int argc,char **argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);QApplication::setStyle(QStyleFactory::create("Fusion"));
    app.setOrganizationName("PixelWorkshopTests");app.setApplicationName("UnsavedChanges");
    QTemporaryDir temp(QDir::currentPath()+"/unsaved-tests-XXXXXX");QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    try {
        require(temp.isValid(),"Temporary folder failed");
        const auto first=temp.filePath("原图.png"),second=temp.filePath("另一张.png");
        QImage source(64,40,QImage::Format_RGB888);source.fill(Qt::red);require(source.save(first),"Save fixture failed");
        source.fill(Qt::blue);require(source.save(second),"Save fixture failed");
        MainWindow window;window.show();open(window,first);
        require(!window.isWindowModified(),"Opened image marked dirty");
        auto idle=[&]{return !window.property("processingBusy").toBool();};
        QMetaObject::invokeMethod(&window,"converToGrayscale");waitFor(idle);
        require(window.isWindowModified(),"Edited image lacks dirty marker");
        answerClose(window,QMessageBox::Cancel);require(window.isVisible(),"Cancel closed window");
        open(window,second,QMessageBox::Cancel);
        require(window.windowTitle().contains("原图.png") && window.isWindowModified(),"Cancel replaced current image");
        QTimer::singleShot(0,&window,[&] {auto *picker=window.findChild<QFileDialog *>();require(picker,"Missing save picker");picker->reject();});
        QMetaObject::invokeMethod(&window,"saveImage");require(window.isWindowModified(),"Cancelled save cleared dirty flag");
        const auto saved=temp.filePath("结果.png");
        QTimer::singleShot(0,&window,[&] {auto *picker=window.findChild<QFileDialog *>();require(picker,"Missing save picker");picker->selectFile(saved);QMetaObject::invokeMethod(picker,"accept");});
        QMetaObject::invokeMethod(&window,"saveImage");
        require(!window.isWindowModified() && QImage(saved).pixelColor(0,0).red()==76,"Successful save did not mark the correct state clean");
        window.findChild<QAction *>("undoAction")->trigger();waitFor(idle);
        require(window.isWindowModified(),"Undo away from saved version marked clean");
        window.findChild<QAction *>("redoAction")->trigger();waitFor(idle);
        require(!window.isWindowModified(),"Redo to saved version marked dirty");
        QMetaObject::invokeMethod(&window,"restoreOriginal");waitFor(idle);
        open(window,second,QMessageBox::Discard);
        require(!window.isWindowModified() && window.windowTitle().contains("另一张.png"),"Discard did not replace and clear history");
        QMetaObject::invokeMethod(&window,"showToneDialog");
        auto *editor=window.findChild<EditorPage *>();
        auto ready=[&]{return editor->findChild<QPushButton *>("confirmButton",Qt::FindDirectChildrenOnly)->isEnabled();};
        waitFor(ready);editor->findChild<QSlider *>("brightnessSlider")->setValue(15);waitFor(ready);
        require(window.isWindowModified(),"Uncommitted editor draft lacks protection");
        answerClose(window,QMessageBox::Cancel);require(window.isVisible() && editor->isVisible(),"Cancel discarded draft");
        QTimer pickerPoll;
        QObject::connect(&pickerPoll,&QTimer::timeout,&window,[&] {
            if(auto *picker=window.findChild<QFileDialog *>()) {pickerPoll.stop();picker->reject();}
        });
        pickerPoll.start(10);answerClose(window,QMessageBox::Save);waitFor(idle);
        QApplication::processEvents();
        require(window.isVisible() && window.isWindowModified(),"Cancel save during close discarded the draft or closed window");
        QObject::disconnect(&pickerPoll,nullptr,&window,nullptr);
        QObject::connect(&pickerPoll,&QTimer::timeout,&window,[&] {
            if(auto *picker=window.findChild<QFileDialog *>()) {
                pickerPoll.stop();picker->selectFile(temp.filePath("草稿.png"));QMetaObject::invokeMethod(picker,"accept");
            }
        });
        pickerPoll.start(10);answerClose(window,QMessageBox::Save);waitFor([&]{return !window.isVisible();});
        require(QImage(temp.filePath("草稿.png")).pixelColor(0,0)==QColor(15,15,255),"Closing saved the old committed image instead of the draft");
        std::cout<<"PASS: dirty state, cancel close/open/save, save clean marker, undo/redo, discard and save editor draft before closing\n";
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
