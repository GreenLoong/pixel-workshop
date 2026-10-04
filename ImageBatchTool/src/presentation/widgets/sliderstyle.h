#ifndef SLIDERSTYLE_H
#define SLIDERSTYLE_H

#include <QProxyStyle>
#include <QSlider>
#include <QStyleFactory>

// 只调整轨道点击策略；数值映射、拖动和键盘交互仍由 Qt 处理。
class AbsoluteSliderStyle final : public QProxyStyle
{
public:
    AbsoluteSliderStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *data = nullptr) const override
    {
        if (hint == SH_Slider_AbsoluteSetButtons)
            return Qt::LeftButton;
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
    static void applyTo(QSlider *slider)
    {
        auto *style = new AbsoluteSliderStyle;
        style->setParent(slider);
        slider->setStyle(style);
        slider->setTracking(true);
    }
};
#endif
