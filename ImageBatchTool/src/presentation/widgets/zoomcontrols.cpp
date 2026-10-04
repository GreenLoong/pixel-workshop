#include "presentation/widgets/zoomcontrols.h"
#include "presentation/widgets/previewlabel.h"
#include "presentation/widgets/sliderstyle.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QToolButton>

ZoomControls::ZoomControls(PreviewLabel *preview,QWidget *parent):QWidget(parent)
{
    this->setObjectName("zoomControls");
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(8);

    auto *fit = new QToolButton(this);
    fit->setObjectName("fitPreviewButton");
    fit->setText("适应 / 100%");
    fit->setFixedHeight(30);
    fit->setToolTip("切换适应窗口与实际大小，并将图片居中");
    auto *percent = new QComboBox(this);
    percent->setObjectName("zoomPercent");
    percent->setFixedWidth(112);
    percent->setCursor(Qt::PointingHandCursor);
    percent->setItemDelegate(new QStyledItemDelegate(percent));
    percent->setEditable(true);
    percent->lineEdit()->setTextMargins(0, 0, 0, 0);
    percent->lineEdit()->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    percent->setInsertPolicy(QComboBox::NoInsert);
    percent->setMaxVisibleItems(12);
    percent->lineEdit()->setValidator(new QRegularExpressionValidator(
        QRegularExpression("(?:[1-9][0-9]?|[1-7][0-9]{2}|800)%?"), percent));
    percent->setToolTip("选择或输入 1%～800% 的预览比例");
    for (int value : {800, 700, 600, 500, 400, 300, 200, 100, 75, 50, 25, 10})
        percent->addItem(QString("%1%").arg(value), value);
    percent->setCurrentIndex(percent->findData(100));
    auto *minus = new QToolButton(this);
    minus->setObjectName("zoomOutButton");
    minus->setText("−");
    minus->setFixedSize(30,30);
    minus->setToolTip("缩小预览");
    auto *slider = new QSlider(Qt::Horizontal, this);
    AbsoluteSliderStyle::applyTo(slider);
    slider->setObjectName("zoomSlider");
    slider->setRange(1, 800);
    slider->setValue(100);
    slider->setFixedWidth(130);
    slider->setToolTip("预览缩放，1%～800%");
    auto *plus = new QToolButton(this);
    plus->setObjectName("zoomInButton");
    plus->setText("+");
    plus->setFixedSize(30,30);
    plus->setToolTip("放大预览");
    for (QWidget *widget : QList<QWidget *>{fit, percent, minus, slider, plus})
        layout->addWidget(widget);
    this->setEnabled(false);

    connect(fit, &QToolButton::clicked, preview, &PreviewLabel::toggleFitActual);
    connect(minus, &QToolButton::clicked, preview, &PreviewLabel::zoomOut);
    connect(plus, &QToolButton::clicked, preview, &PreviewLabel::zoomIn);
    connect(slider, &QSlider::valueChanged, preview, &PreviewLabel::setZoomPercent);
    connect(percent, &QComboBox::activated, this, [preview, percent](int index) {
        preview->setZoomPercent(percent->itemData(index).toInt());
    });
    connect(percent->lineEdit(), &QLineEdit::editingFinished, this, [preview, percent] {
        QString text = percent->currentText();
        text.remove('%');
        bool valid = false;
        const int value = text.toInt(&valid);
        if (valid && value >= 1 && value <= 800)
            preview->setZoomPercent(value);
        else
            percent->setEditText(QString("%1%").arg(preview->zoomPercent()));
    });
    connect(preview, &PreviewLabel::imageAvailable, this, &QWidget::setEnabled);
    connect(preview, &PreviewLabel::zoomChanged, this, [percent, slider](int value) {
        const QSignalBlocker sliderBlocker(slider);
        const QSignalBlocker percentBlocker(percent);
        slider->setValue(value);
        // 任意比例显示在输入框中，预设列表始终保持固定。
        percent->setCurrentIndex(percent->findData(value));
        percent->setEditText(QString("%1%").arg(value));
        percent->lineEdit()->setCursorPosition(0);
    });

}
