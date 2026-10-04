#include "presentation/dialogs/batchdialog.h"
#include "presentation/widgets/dialogappearance.h"
#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDir>

BatchDialog::BatchDialog(const ImageProcessor::Options &options,QWidget *parent)
    :QDialog(parent),options_(options),job_(this),input_(new QLineEdit(this)),output_(new QLineEdit(this)),
      status_(new QLabel(this)),progress_(new QProgressBar(this)),start_(new QPushButton("开始处理",this)),
      cancel_(new QPushButton("取消任务",this)),report_(new QPushButton("导出报告",this)),table_(new QTableWidget(this))
{
    setObjectName("BatchDialog");resize(960,700);setMinimumSize(780,560);
    auto *root=new QVBoxLayout(this);root->setContentsMargins(12,12,12,12);root->setSpacing(0);
    auto *content=new QWidget(this);content->setObjectName("contentPanel");root->addWidget(content,1);
    auto *body=new QVBoxLayout(content);body->setContentsMargins(24,20,24,20);body->setSpacing(16);
    auto *heading=new QLabel("文件夹批量处理",this);heading->setObjectName("dialogHeading");
    auto *header=new QHBoxLayout;header->addWidget(heading,1);
    auto *close=new QToolButton(this);close->setObjectName("closeButton");close->setIcon(QIcon(":/indicators/close.svg"));
    close->setFixedSize(32,32);header->addWidget(close);body->addLayout(header);
    auto *hint=new QLabel("使用打开本页时的编辑参数，逐张处理所选目录中的图片。输出 PNG，保留原文件，同名自动编号。",this);
    hint->setObjectName("modeHint");hint->setWordWrap(true);body->addWidget(hint);
    QSettings settings;
    input_->setObjectName("batchInput");output_->setObjectName("batchOutput");
    input_->setText(settings.value("batch/input").toString());output_->setText(settings.value("batch/output").toString());
    const auto directoryRow=[&](QString text,QLineEdit *edit) {
        auto *row=new QHBoxLayout;row->addWidget(new QLabel(text,this));row->addWidget(edit,1);
        auto *choose=new QPushButton("选择文件夹",this);choose->setProperty("directoryChooser",true);choose->setAutoDefault(false);row->addWidget(choose);
        connect(choose,&QPushButton::clicked,this,[this,edit,text] {
            const QString path=QFileDialog::getExistingDirectory(this,text,edit->text());if(!path.isEmpty())edit->setText(path);
        });body->addLayout(row);
    };
    directoryRow("输入目录",input_);directoryRow("输出目录",output_);
    QStringList summary;
    if(options.grayscale)summary<<"灰度";
    if(options.targetSize!=cv::Size())summary<<QString("尺寸 %1 × %2").arg(options.targetSize.width).arg(options.targetSize.height);
    if(options.rotation!=0 || options.flipHorizontal || options.flipVertical || options.crop!=cv::Rect2d())summary<<"裁剪 / 旋转 / 翻转";
    if(options.hasColorAdjustments() || options.brightness!=0 || options.contrast!=1)summary<<"颜色与光线";
    if(options.background!=ImageProcessor::BackgroundMode::None)summary<<"背景编辑";
    auto *parameters=new QLabel("本次参数："+(summary.isEmpty()?QString("保持原图，转换为 PNG"):summary.join(" · ")),this);
    parameters->setWordWrap(true);body->addWidget(parameters);
    status_->setText("等待开始。不递归扫描子目录；批量裁剪与画笔按每张图片的相对位置应用。");
    status_->setWordWrap(true);body->addWidget(status_);progress_->setObjectName("batchProgress");
    progress_->setRange(0,1);progress_->setValue(0);body->addWidget(progress_);
    table_->setColumnCount(3);table_->setHorizontalHeaderLabels({"输入图片","输出文件","结果 / 失败原因"});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->verticalHeader()->hide();body->addWidget(table_,1);
    auto *footer=new QWidget(this);footer->setObjectName("footerPanel");root->addWidget(footer);
    auto *foot=new QHBoxLayout(footer);foot->setContentsMargins(24,16,24,16);foot->addWidget(report_);foot->addStretch();
    auto *exit=new QPushButton("关闭",this);exit->setAutoDefault(false);foot->addWidget(cancel_);foot->addWidget(start_);foot->addWidget(exit);
    start_->setObjectName("confirmButton");start_->setAutoDefault(false);cancel_->setObjectName("cancelBatchButton");
    report_->setEnabled(false);setRunning(false);DialogAppearance::setup(this,{heading});
    connect(start_,&QPushButton::clicked,this,&BatchDialog::start);
    connect(report_,&QPushButton::clicked,this,&BatchDialog::exportReport);
    connect(close,&QToolButton::clicked,this,&BatchDialog::reject);connect(exit,&QPushButton::clicked,this,&BatchDialog::reject);
    connect(cancel_,&QPushButton::clicked,this,[this] {job_.cancel();cancel_->setEnabled(false);status_->setText("正在取消，等待当前图片处理结束…");});
    connect(&job_,&BatchJob::planned,this,[this](int total){progress_->setRange(0,std::max(1,total));});
    connect(&job_,&BatchJob::itemFinished,this,[this](BatchItem item,int done,int total) {
        const int row=table_->rowCount();table_->insertRow(row);
        table_->setItem(row,0,new QTableWidgetItem(item.input));table_->setItem(row,1,new QTableWidgetItem(item.output));
        auto *result=new QTableWidgetItem(item.error.isEmpty()?"成功":item.error);
        if(!item.error.isEmpty())result->setForeground(QColor("#d92d20"));table_->setItem(row,2,result);
        progress_->setValue(done);status_->setText(QString("已处理 %1 / %2：%3").arg(done).arg(total).arg(QFileInfo(item.input).fileName()));
    });
    connect(&job_,&BatchJob::finished,this,[this](int total,int success,int failed,bool cancelled,QString error) {
        progress_->setRange(0,std::max(1,total));progress_->setValue(success+failed);
        setRunning(false);report_->setEnabled(table_->rowCount()>0);
        status_->setText(error.isEmpty()?QString("%1。成功 %2，失败 %3，未处理 %4。")
            .arg(cancelled?"任务已取消":"任务完成").arg(success).arg(failed).arg(total-success-failed):"任务未启动："+error);
        if(closeWhenFinished_)QDialog::reject();
    });
}
void BatchDialog::setRunning(bool running) {
    start_->setEnabled(!running);cancel_->setEnabled(running);input_->setEnabled(!running);output_->setEnabled(!running);
    report_->setEnabled(!running && table_->rowCount()>0);
    for(auto *button:findChildren<QPushButton *>())if(button->property("directoryChooser").toBool())button->setEnabled(!running);
}
void BatchDialog::start() {
    if(input_->text().trimmed().isEmpty() || output_->text().trimmed().isEmpty()) {status_->setText("请选择输入和输出文件夹。");return;}
    table_->setRowCount(0);progress_->setRange(0,0);closeWhenFinished_=false;setRunning(true);status_->setText("正在扫描图片…");
    QSettings settings;settings.setValue("batch/input",input_->text().trimmed());settings.setValue("batch/output",output_->text().trimmed());
    job_.start(input_->text().trimmed(),output_->text().trimmed(),options_);
}
void BatchDialog::reject() {
    if(job_.isRunning()) {closeWhenFinished_=true;job_.cancel();cancel_->setEnabled(false);status_->setText("正在取消，完成当前图片后关闭…");return;}
    QDialog::reject();
}
void BatchDialog::exportReport() {
    const QString path=QFileDialog::getSaveFileName(this,"导出处理报告","batch-report.csv","CSV 文件 (*.csv)");if(path.isEmpty())return;
    QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate)) {QMessageBox::warning(this,"导出失败",file.errorString());return;}
    file.write("\xEF\xBB\xBF");QTextStream out(&file);out<<"输入图片,输出文件,处理结果\n";
    for(int row=0;row<table_->rowCount();++row) {
        QStringList fields;
        for(int col=0;col<3;++col) {QString text=table_->item(row,col)->text();text.replace('"',"\"\"");fields<<'"'+text+'"';}
        out<<fields.join(',')<<'\n';
    }
    out.flush();if(!file.flush() || out.status()!=QTextStream::Ok)QMessageBox::warning(this,"导出失败","报告写入失败，请检查磁盘空间。");
}
