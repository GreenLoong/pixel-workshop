#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QPushButton>
#include <QDebug>
#include <QPixmap>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->openImageButton,&QPushButton::clicked,this,&MainWindow::openImage);
}

MainWindow::~MainWindow()
{
    delete ui;
}

//打开图片
void MainWindow::openImage()
{
    //this 指定当前主窗口作为对话框的父窗口, "选择图片" 对话框标题, QString() 不指定初始目录,  "图片文件 (...)" 筛选显示的文件扩展名
    const QString filePath = QFileDialog::getOpenFileName(this,"选择图片",QString(),"图片文件(*.png *.jpg *.jpeg *.bmp)");

    //用户取消选择时，不改变当前状态
    if(filePath.isEmpty())
    {
        return;
    }

    qInfo() << "选中的图片:" << filePath;

    //先读取到临时对象，确认成功后再更新界面
    QPixmap image;
    if(!image.load(filePath))
    {
        QMessageBox::warning(this,"打开失败","无法读取这张图片,检查文件是否损坏或者格式是否受支持");
        return;
    }

    //根据显示区域大小缩放，保持图片原有比例
    const QPixmap preview = image.scaled(ui->imageLabel->contentsRect().size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);

     ui->imageLabel->setPixmap(preview);
}
