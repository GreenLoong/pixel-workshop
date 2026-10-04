#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPixmap>
#include <QSize>
#include <QString>
#include "imageprocessor.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE
class QUndoStack;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void openImage();
    void saveImage();
    void converToGrayscale();
    void restoreOriginal();
    void showResizeDialog();
    void showToneDialog();
    void showGeometryDialog();
    void showBackgroundDialog();
    void showBatchDialog();
    void showFullScreenPreview();

private:
    void setupMenus();
    void setupWindowControls();
    void updateWindowFrame();
    void setupZoomControls();
    void applyTheme();
    void updatePreview();
    void updateImageInfo();
    void updateActionState();
    void refreshImageUi();
    bool applyProcessing(const ImageProcessor::Options &options, bool recordHistory = true);

    Ui::MainWindow *ui;
    QWidget *windowFrame_ = nullptr;

    QPixmap originalImage;      // 打开时原图
    QPixmap currentImage;       // 当前处理图
    QString currentFilePath;    // 当前图片路径

    ImageProcessor::Options processingOptions_;
    QUndoStack *history_ = nullptr;
};

#endif // MAINWINDOW_H
