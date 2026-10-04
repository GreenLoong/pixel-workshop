#ifndef SELECTIONITEM_H
#define SELECTIONITEM_H
#include <QGraphicsObject>
#include <QRectF>

// 场景坐标中的裁剪框，拖动和尺寸约束集中在此处。
class SelectionItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit SelectionItem(QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override;
    void setBounds(const QRectF &bounds);
    void setSelection(const QRectF &selection);
    QRectF selection() const { return rect_; }
    void setRatio(double ratio);
signals:
    void selectionChanged(QRectF rect);
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
private:
    QRectF bounds_, rect_, pressRect_;
    QPointF pressPoint_;
    double ratio_ = 0;
    int edges_ = 0;
};
#endif
