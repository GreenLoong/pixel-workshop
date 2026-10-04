#ifndef RESIZEPANEL_H
#define RESIZEPANEL_H

#include <QWidget>
#include <QSize>

QT_BEGIN_NAMESPACE
namespace Ui {
class ResizePanel;
}
QT_END_NAMESPACE

class ResizePanel final : public QWidget
{
    Q_OBJECT

public:
    explicit ResizePanel(const QSize &originalSize, const QSize &currentSize, QWidget *parent = nullptr);

    ~ResizePanel() override;

    QSize targetSize() const;
    bool isValid() const;
signals:
    void targetSizeChanged(QSize size);

private:
    void updateMode();
    void updateFromPercent();
    void updateFromWidth();
    void updateFromHeight();
    void updateSummary();
    void resetSize();
    void restoreOriginalSize();
    void setTargetSize(const QSize &size);

    Ui::ResizePanel *ui;
    QSize originalSize_;
    QSize currentSize_;
    QSize aspectSize_;
    QSize lastTargetSize_;

};

#endif // RESIZEPANEL_H
