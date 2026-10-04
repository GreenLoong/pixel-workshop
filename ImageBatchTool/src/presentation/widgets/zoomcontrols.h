#ifndef ZOOMCONTROLS_H
#define ZOOMCONTROLS_H

#include <QWidget>

class PreviewLabel;
// 百分比、滑块与预览的双向同步集中在一个控件中。
class ZoomControls final : public QWidget {
public:
    explicit ZoomControls(PreviewLabel *preview,QWidget *parent=nullptr);
};
#endif
