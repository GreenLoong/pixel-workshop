#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imageprocessor.h"
#include "resizedialog.h"

#include <QFileDialog>
#include <QPushButton>
#include <QDebug>
#include <QPixmap>
#include <QMessageBox>
#include <QEvent>
#include <QSizePolicy>
#include <QImage>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 打开图片按钮
    connect(ui->openImageButton, &QPushButton::clicked, this, &MainWindow::openImage);
    // 灰度化按钮
    connect(ui->grayscaleButton, &QPushButton::clicked, this, &MainWindow::converToGrayscale);
    // 恢复原图按钮
    connect(ui->restoreButton, &QPushButton::clicked, this, &MainWindow::restoreOriginal);
    // 另存为按钮
    connect(ui->saveImageButton, &QPushButton::clicked, this, &MainWindow::saveImage);

    // 让布局决定显示区域大小，避免图片自身尺寸撑大窗口
    ui->imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // 监听图片区域的事件
    ui->imageLabel->installEventFilter(this);

    // 调整大小
    connect(ui->resizeButton, &QPushButton::clicked, this, &MainWindow::showResizeDialog);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 打开图片
void MainWindow::openImage()
{
    // this 指定当前主窗口作为对话框的父窗口, "选择图片" 对话框标题, QString() 不指定初始目录,  "图片文件 (...)" 筛选显示的文件扩展名
    const QString filePath = QFileDialog::getOpenFileName(this, "选择图片", QString(), "图片文件(*.png *.jpg *.jpeg *.bmp)");

    // 用户取消选择时，不改变当前状态
    if (filePath.isEmpty())
    {
        return;
    }

    qInfo() << "选中的图片:" << filePath;

    // 先读取到临时对象，确认成功后再更新界面
    QPixmap image;
    if (!image.load(filePath))
    {
        QMessageBox::warning(this, "打开失败", "无法读取这张图片,检查文件是否损坏或者格式是否受支持");
        return;
    }

    // //根据显示区域大小缩放，保持图片原有比例
    // const QPixmap preview = image.scaled(ui->imageLabel->contentsRect().size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

    //  ui->imageLabel->setPixmap(preview);

    originalImage = image;
    currentImage = image;
    grayscaleEnabled = false;

    updatePreview();
}

// 更新图片大小
void MainWindow::updatePreview()
{
    // 尚未打卡图片时，不进行缩放
    if (currentImage.isNull())
        return;

    const QSize targetSize = ui->imageLabel->contentsRect().size();

    if (targetSize.isEmpty())
        return;

    const QPixmap preview = currentImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    ui->imageLabel->setPixmap(preview);
}

// 事件过滤器
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // 事件属于图片标签, 标签的尺寸发生变化
    if (watched == ui->imageLabel && event->type() == QEvent::Resize)
        updatePreview();

    return QMainWindow::eventFilter(watched, event);
}

// 灰度化
void MainWindow::converToGrayscale()
{
    if (originalImage.isNull())
    {
        QMessageBox::information(this, "提示", "请先打开一张图片");
        return;
    }

    // try
    // {
    //     // 将 Qt 图片统一为8位，三通道 RGB 格式
    //     QImage rgbImage = originalImage.toImage().convertToFormat(QImage::Format_RGB888);

    //     // 暂时借用 QImage 的像素内存，交给 OpenCV 读取
    //     cv::Mat rgb(rgbImage.height(), rgbImage.width(), CV_8UC3, rgbImage.bits(), static_cast<size_t>(rgbImage.bytesPerLine()));

    //     // cv::Mat gray;
    //     // cv::cvtColor(rgb,gray,cv::COLOR_RGB2GRAY);
    //     cv::Mat gray = ImageProcessor::toGrayscale(rgb);

    //     // 将 OpenCV 的单通道灰度结果交给 Qt
    //     QImage grayImage(gray.data, gray.cols, gray.rows, static_cast<qsizetype>(gray.step), QImage::Format_Grayscale8);

    //     // 复制像素，让显示结果不依赖局部 Mat 的生命周期
    //     currentImage = QPixmap::fromImage(grayImage.copy());
    //     updatePreview();
    // }
    // catch (const cv::Exception &error)
    // {
    //     QMessageBox::warning(this, "处理失败", QString::fromUtf8(error.what()));
    // }

    applyProcessing(true, currentImage.size());
}

// 恢复原图
void MainWindow::restoreOriginal()
{
    if (originalImage.isNull())
        return;

    grayscaleEnabled = false;
    currentImage = originalImage;

    //currentImage = originalImage;
    updatePreview();
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

    // 保证输出文件名以 .png 结尾
    if (!outputPath.endsWith(".png", Qt::CaseInsensitive))
        outputPath += ".png";

    // QFile outputFile(outputPath);

    // //仅允许创建新文件，已有文件不会被覆盖
    // if(!outputFile.open(QIODevice::WriteOnly | QIODevice::NewOnly))
    // {
    //     QMessageBox::warning(this,"保存失败","无法创建文件。请确认文件名没有重复，且目录允许写入 \n" + outputFile.errorString());
    //     return;
    // }

    const QString basePath = outputPath.left(outputPath.size() - 4);

    QFile outputFile;
    int number = 1;

    while (true)
    {
        outputFile.setFileName(outputPath);

        // 尝试创建新文件，避免覆盖已有文件
        if (outputFile.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            break;

        // 同名文件存在，生成下一个候选名称
        if (QFile::exists(outputPath))
        {
            outputPath = QString("%1(%2).png").arg(basePath).arg(number);
            ++number;
            continue;
        }

        // 文件不存在却创建失败，可能是因为目录或者权限问题
        QMessageBox::warning(this, "保存失败", "无法保存文件: \n" + outputPath + "\n" + outputFile.errorString());
        return;
    }

    // 保存完整处理结果，不使用标签里的缩小预览
    if (!currentImage.save(&outputFile, "PNG"))
    {
        outputFile.close();

        // 清除本次创建但未保存成功的文件
        const bool removed = outputFile.remove();

        QMessageBox::warning(this, "保存失败", removed ? "图片写入失败，请检查磁盘空间和目录权限" : "图片写入失败，且未能删除不完整文件，请检查输出目录");

        return;
    }

    outputFile.close();

    QMessageBox::information(this, "保存成功", "图片保存到: \n" + outputPath);
}

void MainWindow::showResizeDialog()
{
    if (originalImage.isNull())
    {
        QMessageBox::information( this, "提示", "请先打开一张图片。");
        return;
    }

    ResizeDialog dialog(originalImage.size(), currentImage.size(), this);

    //取消或关闭对话框时，不处理图片
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    applyProcessing(grayscaleEnabled, dialog.targetSize());
}

bool MainWindow::applyProcessing(bool grayscale, const QSize &targetSize)
{
    if (originalImage.isNull())
    {
        return false;
    }

    try {
        //始终从原图重新计算
        QImage rgbImage = originalImage.toImage().convertToFormat(QImage::Format_RGB888);

        cv::Mat rgb(rgbImage.height(), rgbImage.width(), CV_8UC3, rgbImage.bits(), static_cast<size_t>(rgbImage.bytesPerLine()));

        cv::Mat result = rgb;

        if (grayscale)
        {
            result = ImageProcessor::toGrayscale(result);
        }

        result = ImageProcessor::resizeToSize( result, cv::Size(targetSize.width(), targetSize.height()));

        const QImage::Format format = result.channels() == 1 ? QImage::Format_Grayscale8 : QImage::Format_RGB888;

        QImage resultImage( result.data, result.cols, result.rows, static_cast<qsizetype>(result.step), format);

        QPixmap processed = QPixmap::fromImage(resultImage.copy());

        if (processed.isNull())
        {
            QMessageBox::warning( this, "处理失败", "无法创建处理结果图片");
            return false;
        }

        //处理成功后才更新图片和状态
        currentImage = processed;
        grayscaleEnabled = grayscale;

        updatePreview();
        return true;

    }
    catch (const cv::Exception &error)
    {
        QMessageBox::warning( this, "处理失败", QString::fromUtf8(error.what()));
        return false;
    }
}
