#ifndef BRUSHPREVIEW_H
#define BRUSHPREVIEW_H
#include "domain/imageprocessor.h"
#include "presentation/widgets/previewlabel.h"
#include <functional>

class BrushPreview final : public PreviewLabel {
public:
    explicit BrushPreview(QWidget *parent=nullptr);
    bool painting=false,foreground=true;
    double radius=0.025;
    std::function<void(ImageProcessor::BrushStroke)> onStroke;
protected:
    void paintEvent(QPaintEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    void append(QPoint point);
    ImageProcessor::BrushStroke stroke_;
    bool drawing_=false;
    QPoint pointer_;
    bool pointerInside_=false;
};
#endif
