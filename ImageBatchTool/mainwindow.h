#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPixmap>
#include <QSize>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void openImage();
    void updatePreview();
    void converToGrayscale();
    void restoreOriginal();
    void saveImage();
    void showResizeDialog();
    bool applyProcessing(bool grayscale, const QSize &targetSize);

    Ui::MainWindow *ui;

    QPixmap originalImage;  //打开时原图
    QPixmap currentImage;   //当前处理图

    bool grayscaleEnabled = false;


};
#endif // MAINWINDOW_H
