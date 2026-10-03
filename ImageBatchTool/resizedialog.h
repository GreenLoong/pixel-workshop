#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QDialog>
#include <QSize>

QT_BEGIN_NAMESPACE
namespace Ui {
class ResizeDialog;
}
QT_END_NAMESPACE

class ResizeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResizeDialog(const QSize &originalSize, const QSize &currentSize, QWidget *parent = nullptr);

    ~ResizeDialog() override;

    QSize targetSize() const;

private:
    void updateMode();
    void updateFromPercent();
    void updateFromWidth();
    void updateFromHeight();
    void updateSummary();
    void resetSize();
    void restoreOriginalSize();
    void setTargetSize(const QSize &size);

    Ui::ResizeDialog *ui;
    QSize originalSize_;
    QSize currentSize_;
    QSize aspectSize_;
};

#endif // RESIZEDIALOG_H
