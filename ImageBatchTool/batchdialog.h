#ifndef BATCHDIALOG_H
#define BATCHDIALOG_H
#include "batchjob.h"
#include <QDialog>
class QLineEdit;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class BatchDialog final : public QDialog {
    Q_OBJECT
public:
    explicit BatchDialog(const ImageProcessor::Options &options,QWidget *parent=nullptr);
    void reject() override;
private:
    void start();
    void exportReport();
    void setRunning(bool running);
    ImageProcessor::Options options_;
    BatchJob job_;
    QLineEdit *input_,*output_;
    QLabel *status_;
    QProgressBar *progress_;
    QPushButton *start_,*cancel_,*report_;
    QTableWidget *table_;
    bool closeWhenFinished_=false;
};
#endif
