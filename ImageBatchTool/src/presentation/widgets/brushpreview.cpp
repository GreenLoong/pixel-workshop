#include "presentation/widgets/brushpreview.h"
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

BrushPreview::BrushPreview(QWidget *parent):PreviewLabel(parent)
{
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}
void BrushPreview::paintEvent(QPaintEvent *event)
{
    PreviewLabel::paintEvent(event);
    if (!painting || !pointerInside_ || sceneRect().isEmpty()) return;
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    const double r = radius * std::min(sceneRect().width(),sceneRect().height()) * std::abs(transform().m11());
    // 在视口上画笔圈，不受系统光标尺寸上限影响，随图片缩放同步变化。
    p.setPen(QPen(Qt::black,3)); p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(pointer_),r,r);
    p.setPen(QPen(Qt::white,1)); p.drawEllipse(QPointF(pointer_),r,r);
}
void BrushPreview::leaveEvent(QEvent *event)
{
    pointerInside_=false; viewport()->update();
    PreviewLabel::leaveEvent(event);
}

void BrushPreview::append(QPoint point) {
    const QRectF b=sceneRect();
    const QPointF p=mapToScene(point);
    if(!b.contains(p))return;
    stroke_.points.emplace_back((p.x()-b.x())/b.width(),(p.y()-b.y())/b.height());
    // 反馈本次画笔轨迹，结束后由结果预览替换。
    const double r=radius*std::min(b.width(),b.height());
    auto *mark=scene()->addEllipse(p.x()-r,p.y()-r,2*r,2*r,QPen(Qt::NoPen),
                                  QColor(foreground?QColor(30,180,100,110):QColor(230,60,80,110)));
    mark->setData(0,"brushMark");mark->setZValue(10);
}
void BrushPreview::mousePressEvent(QMouseEvent *e) {
    if(painting && e->button()==Qt::LeftButton && sceneRect().contains(mapToScene(e->position().toPoint()))) {
        stroke_={};stroke_.foreground=foreground;stroke_.radius=radius;drawing_=true;
        append(e->position().toPoint());e->accept();return;
    }
    PreviewLabel::mousePressEvent(e);
}
void BrushPreview::mouseMoveEvent(QMouseEvent *e) {
    pointer_=e->position().toPoint();pointerInside_=viewport()->rect().contains(pointer_);
    if(painting)viewport()->setCursor(Qt::CrossCursor);
    viewport()->update();
    if(drawing_){append(e->position().toPoint());e->accept();return;}
    PreviewLabel::mouseMoveEvent(e);
}
void BrushPreview::mouseReleaseEvent(QMouseEvent *e) {
    if(drawing_ && e->button()==Qt::LeftButton) {
        append(e->position().toPoint());drawing_=false;
        for(auto *item:scene()->items())if(item->data(0).toString()=="brushMark")delete item;
        if(!stroke_.points.empty() && onStroke)onStroke(std::move(stroke_));
        e->accept();return;
    }
    PreviewLabel::mouseReleaseEvent(e);
}
