#ifndef PREVIEWLABEL_H
#define PREVIEWLABEL_H

#include <QGraphicsView>
#include <QPixmap>

class QGraphicsPixmapItem;
class QLabel;

// 使用 Qt 图形视图提供裁剪、拖动和滚动范围限制。
class PreviewLabel : public QGraphicsView
{
    Q_OBJECT
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius)
public:
    explicit PreviewLabel(QWidget *parent = nullptr);
    void setImage(const QPixmap &image);
    void setKeepViewOnImageChange(bool keep) { keepViewOnImageChange_=keep; }
    int zoomPercent() const;
    int cornerRadius() const { return cornerRadius_; }
    void setCornerRadius(int radius);

public slots:
    void setZoomPercent(int percent);
    void fitToWindow();
    void toggleFitActual();
    void zoomIn();
    void zoomOut();

signals:
    void zoomChanged(int percent);
    void imageAvailable(bool available);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void applyZoom(double scale);
    void updateViewportMask();
    QGraphicsPixmapItem *imageItem_;
    QLabel *placeholder_;
    double scale_ = 1.0;
    bool fitMode_ = true;
    bool keepViewOnImageChange_ = false;
    int cornerRadius_ = 0; // 全屏预览默认没有圆角；普通预览由样式指定。
};
#endif
