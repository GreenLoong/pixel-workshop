#include "presentation/widgets/selectionitem.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneHoverEvent>
#include <algorithm>

SelectionItem::SelectionItem(QGraphicsItem *parent) : QGraphicsObject(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setZValue(10);
    setAcceptHoverEvents(true);
    setCursor(QCursor(Qt::ArrowCursor));
}
double SelectionItem::hitTolerance() const
{
    const auto views = scene() ? scene()->views() : QList<QGraphicsView *>();
    return 8.0 / (views.isEmpty() ? 1.0 : std::max(.01, std::abs(views.first()->transform().m11())));
}
int SelectionItem::edgesAt(QPointF p) const
{
    const double t = hitTolerance();
    if (!rect_.adjusted(-t,-t,t,t).contains(p)) return 0;
    int edges = 0;
    if (std::abs(p.x()-rect_.left()) < t) edges |= 1;
    else if (std::abs(p.x()-rect_.right()) < t) edges |= 2;
    if (std::abs(p.y()-rect_.top()) < t) edges |= 4;
    else if (std::abs(p.y()-rect_.bottom()) < t) edges |= 8;
    return edges;
}
void SelectionItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const int edges = edgesAt(event->pos());
    Qt::CursorShape cursor = rect_.contains(event->pos()) ? Qt::SizeAllCursor : Qt::ArrowCursor;
    if ((edges & 3) && (edges & 12))
        cursor = edges == 5 || edges == 10 ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
    else if (edges & 3) cursor = Qt::SizeHorCursor;
    else if (edges & 12) cursor = Qt::SizeVerCursor;
    setCursor(QCursor(cursor));
    event->accept();
}
void SelectionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(QCursor(Qt::ArrowCursor));
    QGraphicsObject::hoverLeaveEvent(event);
}
QRectF SelectionItem::boundingRect() const { return bounds_.adjusted(-8,-8,8,8); }
void SelectionItem::setBounds(const QRectF &bounds)
{
    prepareGeometryChange();
    bounds_ = bounds;
    setSelection(bounds);
}
void SelectionItem::setSelection(const QRectF &selection)
{
    rect_ = selection.intersected(bounds_);
    update();
    emit selectionChanged(rect_);
}
void SelectionItem::setRatio(double ratio)
{
    ratio_ = ratio;
    if (ratio > 0) {
        QSizeF size = rect_.size();
        if (size.width()/size.height() > ratio) size.setWidth(size.height()*ratio);
        else size.setHeight(size.width()/ratio);
        setSelection(QRectF(rect_.center()-QPointF(size.width()/2,size.height()/2),size));
    }
}
void SelectionItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    QPainterPath shade;
    shade.addRect(bounds_);
    shade.addRect(rect_);
    shade.setFillRule(Qt::OddEvenFill);
    p->fillPath(shade, QColor(0,0,0,110));
    QPen border(QColor("#2f6bff"), 2); border.setCosmetic(true);
    p->setPen(border); p->setBrush(Qt::NoBrush); p->drawRect(rect_);
    QPen grid(QColor(255,255,255,170),1,Qt::DashLine); grid.setCosmetic(true);
    p->setPen(grid);
    for (int i=1;i<=2;++i) {
        p->drawLine(QPointF(rect_.left()+rect_.width()*i/3,rect_.top()),
                    QPointF(rect_.left()+rect_.width()*i/3,rect_.bottom()));
        p->drawLine(QPointF(rect_.left(),rect_.top()+rect_.height()*i/3),
                    QPointF(rect_.right(),rect_.top()+rect_.height()*i/3));
    }
    p->setPen(border); p->setBrush(Qt::white);
    for (auto point : {rect_.topLeft(),rect_.topRight(),rect_.bottomLeft(),rect_.bottomRight(),
        QPointF(rect_.center().x(),rect_.top()),QPointF(rect_.center().x(),rect_.bottom()),
        QPointF(rect_.left(),rect_.center().y()),QPointF(rect_.right(),rect_.center().y())})
        p->drawRect(QRectF(point-QPointF(hitTolerance()/2,hitTolerance()/2),QSizeF(hitTolerance(),hitTolerance())));
}
void SelectionItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    const QPointF point = event->pos();
    const double tolerance = hitTolerance();
    edges_ = edgesAt(point);
    if (!rect_.adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(point)) {
        event->ignore(); return;
    }
    pressPoint_ = point;
    pressRect_ = rect_;
    event->accept();
}
void SelectionItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    const QPointF delta = event->pos()-pressPoint_;
    QRectF next = pressRect_;
    if (!edges_) {
        next.translate(delta);
        next.moveLeft(std::clamp(next.left(),bounds_.left(),bounds_.right()-next.width()));
        next.moveTop(std::clamp(next.top(),bounds_.top(),bounds_.bottom()-next.height()));
    } else {
        const double minWidth = std::min(8.0,bounds_.width());
        const double minHeight = std::min(8.0,bounds_.height());
        if (edges_&1) next.setLeft(std::clamp(next.left()+delta.x(),bounds_.left(),next.right()-minWidth));
        if (edges_&2) next.setRight(std::clamp(next.right()+delta.x(),next.left()+minWidth,bounds_.right()));
        if (edges_&4) next.setTop(std::clamp(next.top()+delta.y(),bounds_.top(),next.bottom()-minHeight));
        if (edges_&8) next.setBottom(std::clamp(next.bottom()+delta.y(),next.top()+minHeight,bounds_.bottom()));
        if (ratio_>0) {
            const bool horizontal = (edges_&3)!=0;
            double width = horizontal ? next.width() : next.height()*ratio_;
            double height = width/ratio_;
            const QPointF anchor((edges_&1)?pressRect_.right():pressRect_.left(),
                                 (edges_&4)?pressRect_.bottom():pressRect_.top());
            const double maxW = (edges_&1)?anchor.x()-bounds_.left():bounds_.right()-anchor.x();
            const double maxH = (edges_&4)?anchor.y()-bounds_.top():bounds_.bottom()-anchor.y();
            const double factor = std::min({1.0,maxW/width,maxH/height});
            width *= factor; height *= factor;
            next = QRectF(QPointF((edges_&1)?anchor.x()-width:anchor.x(),
                                  (edges_&4)?anchor.y()-height:anchor.y()),QSizeF(width,height));
        }
    }
    setSelection(next);
    event->accept();
}
void SelectionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { event->accept(); }
