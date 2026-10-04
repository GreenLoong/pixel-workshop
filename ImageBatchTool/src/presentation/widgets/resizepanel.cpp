#include "presentation/widgets/resizepanel.h"
#include "ui_resizepanel.h"
#include "domain/imageprocessor.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
int toPixelCount(double value)
{
    const double limited = std::clamp(value, 1.0, static_cast<double>(std::numeric_limits<int>::max()));
    return static_cast<int>(std::llround(limited));
}

} // namespace

ResizePanel::ResizePanel(const QSize &originalSize, const QSize &currentSize, QWidget *parent)
    : QWidget(parent), ui(new Ui::ResizePanel), originalSize_(originalSize),
      currentSize_(currentSize), aspectSize_(currentSize)
{
    ui->setupUi(this);

    ui->resetSizeButton->setAutoDefault(false);
    ui->restoreOriginalSizeButton->setAutoDefault(false);
    ui->widthSpinBox->setValue(currentSize.width());
    ui->heightSpinBox->setValue(currentSize.height());

    ui->originalSizeLabel->setText(QString("原图尺寸：%1 × %2 px").arg(originalSize_.width()).arg(originalSize_.height()));
    ui->currentSizeLabel->setText(QString("当前尺寸：%1 × %2 px").arg(currentSize_.width()).arg(currentSize_.height()));

    // 槽函数链接-按百分比
    connect(ui->percentRadioButton, &QRadioButton::toggled, this, [this](bool)
            { updateMode(); });
    connect(ui->percentSpinBox, &QSpinBox::valueChanged, this, [this](int)
            { if (ui->percentRadioButton->isChecked()) updateFromPercent(); });

    // 槽函数链接-按像素
    connect(ui->widthSpinBox, &QSpinBox::valueChanged, this, [this](int)
            { updateFromWidth(); });
    connect(ui->heightSpinBox, &QSpinBox::valueChanged, this, [this](int)
            { updateFromHeight(); });
    connect(ui->keepAspectCheckBox, &QCheckBox::toggled, this, [this](bool checked)
            {
                if (checked) {
                    // 重新勾选时，以当前宽度为准计算高度。
                    updateFromWidth();
                } });

    connect(ui->resetSizeButton, &QPushButton::clicked, this, &ResizePanel::resetSize);
    connect(ui->restoreOriginalSizeButton, &QPushButton::clicked,
            this, &ResizePanel::restoreOriginalSize);

    updateMode();
}

ResizePanel::~ResizePanel()
{
    delete ui;
}
QSize ResizePanel::targetSize() const
{
    return QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
}

bool ResizePanel::isValid() const
{
    const QSize size = targetSize();
    return ImageProcessor::validOutputSize(cv::Size(size.width(), size.height()));
}

// 模式
void ResizePanel::updateMode()
{
    const bool percentMode = ui->percentRadioButton->isChecked();

    ui->percentSpinBox->setEnabled(percentMode);
    ui->widthSpinBox->setEnabled(!percentMode);
    ui->heightSpinBox->setEnabled(!percentMode);
    ui->keepAspectCheckBox->setEnabled(!percentMode);

    if (percentMode)
        updateFromPercent();
    else
        updateSummary();
}

// 按百分比模式
void ResizePanel::updateFromPercent()
{
    const double scale = ui->percentSpinBox->value() / 100.0;

    // 批量更新宽高时不触发输入框之间的联动。
    const QSignalBlocker widthBlocker(ui->widthSpinBox);
    const QSignalBlocker heightBlocker(ui->heightSpinBox);

    aspectSize_ = currentSize_;
    ui->widthSpinBox->setValue(toPixelCount(currentSize_.width() * scale));
    ui->heightSpinBox->setValue(toPixelCount(currentSize_.height() * scale));

    updateSummary();
}

// 保持比例按像素模式 - 宽度
void ResizePanel::updateFromWidth()
{
    if (ui->pixelRadioButton->isChecked() && ui->keepAspectCheckBox->isChecked())
    {
        // 设置高度时不反向触发宽度计算。
        const QSignalBlocker blocker(ui->heightSpinBox);

        const double height = ui->widthSpinBox->value()
            * static_cast<double>(aspectSize_.height()) / aspectSize_.width();

        ui->heightSpinBox->setValue(toPixelCount(height));
    }

    updateSummary();
}

// 保持比例按像素模式 - 高度
void ResizePanel::updateFromHeight()
{
    if (ui->pixelRadioButton->isChecked() && ui->keepAspectCheckBox->isChecked())
    {

        const QSignalBlocker blocker(ui->widthSpinBox);

        const double width =
            ui->heightSpinBox->value() * static_cast<double>(aspectSize_.width()) / aspectSize_.height();

        ui->widthSpinBox->setValue(toPixelCount(width));
    }

    updateSummary();
}

void ResizePanel::updateSummary()
{
    const QSize size = targetSize();

    const bool allowed = isValid();

    const QString sizeText = QString("目标尺寸：%1 × %2 px").arg(size.width()).arg(size.height());

    // 摘要区在 .ui 中预留提示高度，超限时不推动下面的控件。
    ui->targetSizeLabel->setText(allowed
                                     ? sizeText
                                     : sizeText + "\n超过本版 4000 万像素的输出限制");

    if(size!=lastTargetSize_) {
        lastTargetSize_=size;
        emit targetSizeChanged(size);
    }

}

void ResizePanel::resetSize()
{
    // 回到本次进入尺寸模式时的尺寸。
    setTargetSize(currentSize_);
}

void ResizePanel::restoreOriginalSize()
{
    // 原图尺寸可能超出相对当前尺寸的百分比范围，使用像素模式。
    setTargetSize(originalSize_);
}

void ResizePanel::setTargetSize(const QSize &size)
{
    aspectSize_ = size;
    {
        const QSignalBlocker widthBlocker(ui->widthSpinBox);
        const QSignalBlocker heightBlocker(ui->heightSpinBox);
        const QSignalBlocker checkBlocker(ui->keepAspectCheckBox);
        const QSignalBlocker percentBlocker(ui->percentSpinBox);
        const QSignalBlocker percentModeBlocker(ui->percentRadioButton);
        const QSignalBlocker pixelModeBlocker(ui->pixelRadioButton);

        ui->pixelRadioButton->setChecked(true);
        ui->widthSpinBox->setValue(size.width());
        ui->heightSpinBox->setValue(size.height());
        ui->keepAspectCheckBox->setChecked(true);
        ui->percentSpinBox->setValue(100);
    }
    updateMode();
}
