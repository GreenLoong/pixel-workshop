#include "previewlabel.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRegion>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

PreviewLabel::PreviewLabel(QWidget *parent)
    : QGraphicsView(parent), imageItem_(nullptr), placeholder_(new QLabel(viewport()))
{
    auto *imageScene = new QGraphicsScene(this);
    setScene(imageScene);
    imageItem_ = imageScene->addPixmap(QPixmap());
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setMinimumSize(160, 120);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);

    placeholder_->setText("当前还没有图片\n点击右侧“打开图片”开始");
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setAttribute(Qt::WA_TransparentForMouseEvents);
    placeholder_->setStyleSheet("background: transparent; color: #98a1ae; border: none; font-size: 13px;");
}

void PreviewLabel::setImage(const QPixmap &image)
{
    const QSizeF oldSize = imageItem_->boundingRect().size();
    imageItem_->setPixmap(image);
    // 场景范围只包含图片。Qt 自动限制拖动；较小的方向始终居中。
    setSceneRect(imageItem_->boundingRect());
    placeholder_->setVisible(image.isNull());
    setDragMode(image.isNull() ? QGraphicsView::NoDrag : QGraphicsView::ScrollHandDrag);
    emit imageAvailable(!image.isNull());
    if (!image.isNull() && (fitMode_ || oldSize.isEmpty()
        || (!keepViewOnImageChange_ && oldSize != imageItem_->boundingRect().size())))
        fitToWindow();
}

void PreviewLabel::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter,rect);
    if(!imageItem_->pixmap().hasAlphaChannel())return;
    QPixmap tile(16,16);tile.fill(QColor("#ffffff"));
    QPainter p(&tile);p.fillRect(0,0,8,8,QColor("#dce2ea"));p.fillRect(8,8,8,8,QColor("#dce2ea"));p.end();
    painter->fillRect(imageItem_->boundingRect().intersected(rect),QBrush(tile));
}

int PreviewLabel::zoomPercent() const
{
    return static_cast<int>(std::lround(scale_ * 100));
}

QSizeF PreviewLabel::displayedImageSize() const
{
    return imageItem_->boundingRect().size() * scale_;
}

void PreviewLabel::applyZoom(double scale)
{
    if (imageItem_->pixmap().isNull())
        return;
    scale_ = std::clamp(scale, 0.01, 8.0);
    // 直接设置绝对比例，避免多次乘除积累误差，也不生成放大的位图。
    setTransform(QTransform::fromScale(scale_, scale_));
    emit zoomChanged(zoomPercent());
}

void PreviewLabel::setZoomPercent(int percent)
{
    fitMode_ = false;
    applyZoom(percent / 100.0);
}

void PreviewLabel::fitToWindow()
{
    fitMode_ = true;
    const QSizeF size = imageItem_->boundingRect().size();
    if (size.isEmpty())
        return;
    applyZoom(std::min((viewport()->width() - 2.0) / size.width(),
                      (viewport()->height() - 2.0) / size.height()));
    centerOn(imageItem_);
}

void PreviewLabel::toggleFitActual()
{
    if (qFuzzyCompare(scale_, 1.0))
        fitToWindow();
    else {
        setZoomPercent(100);
        centerOn(imageItem_);
    }
}

void PreviewLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !imageItem_->pixmap().isNull()
        && imageItem_->contains(imageItem_->mapFromScene(mapToScene(event->position().toPoint())))) {
        toggleFitActual();
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void PreviewLabel::zoomIn()
{
    setZoomPercent(std::max(zoomPercent() + 1,
                           static_cast<int>(std::lround(zoomPercent() * 1.2))));
}

void PreviewLabel::zoomOut()
{
    setZoomPercent(std::min(zoomPercent() - 1,
                           static_cast<int>(std::lround(zoomPercent() / 1.2))));
}

void PreviewLabel::wheelEvent(QWheelEvent *event)
{
    // 事件由整个视口接收，图片周围的留白也允许缩放。
    if (imageItem_->pixmap().isNull())
    {
        event->ignore();
        return;
    }
    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
        event->ignore();
        return;
    }
    fitMode_ = false;
    applyZoom(scale_ * std::pow(1.2, delta / 120.0));
    event->accept();
}

void PreviewLabel::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    updateViewportMask();
    placeholder_->setGeometry(viewport()->rect());
    if (fitMode_)
        fitToWindow();
}

void PreviewLabel::setCornerRadius(int radius)
{
    cornerRadius_ = std::max(0, radius);
    updateViewportMask();
}

void PreviewLabel::updateViewportMask()
{
    if (cornerRadius_ == 0) {
        viewport()->clearMask();
        return;
    }
    // 样式圆角只绘制边框，视口还需要单独裁剪，避免放大图片覆盖四角。
    // 遮罩属于视口，不参与图像数据、缩放比例和滚动范围计算。
    QPainterPath path;
    const int innerRadius = std::max(0, cornerRadius_ - frameWidth());
    path.addRoundedRect(QRectF(viewport()->rect()), innerRadius, innerRadius);
    viewport()->setMask(QRegion(path.toFillPolygon().toPolygon()));
}
