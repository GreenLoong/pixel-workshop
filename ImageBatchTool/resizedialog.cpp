#include "resizedialog.h"
#include "ui_resizedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QLayout>
#include <QStyle>

#include <algorithm>
#include <cmath>
#include <limits>

int toPixelCount(double value)
{
    const double limited = std::clamp(value, 1.0, static_cast<double>(std::numeric_limits<int>::max()));
    return static_cast<int>(std::llround(limited));
}

ResizeDialog::ResizeDialog(const QSize &originalSize, const QSize &currentSize, QWidget *parent)
    : QDialog(parent), ui(new Ui::ResizeDialog), originalSize_(originalSize),
      currentSize_(currentSize), aspectSize_(currentSize)
{
    ui->setupUi(this);

    auto *confirmButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    auto *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    confirmButton->setObjectName("confirmButton");
    confirmButton->style()->unpolish(confirmButton);
    confirmButton->style()->polish(confirmButton);
    confirmButton->setText("确定");
    cancelButton->setText("取消");
    confirmButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cancelButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 移除按钮盒默认的左侧伸缩空间，让底部两个按钮均分宽度。
    auto *buttonLayout = ui->buttonBox->layout();
    for (int i = 0; i < buttonLayout->count(); ++i) {
        if (auto *spacer = buttonLayout->itemAt(i)->spacerItem()) {
            spacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        }
    }
    buttonLayout->invalidate();
    confirmButton->setDefault(true);
    ui->resetSizeButton->setAutoDefault(false);
    ui->restoreOriginalSizeButton->setAutoDefault(false);

    // 保留键盘方向键调节，使用简洁的无箭头输入框。
    for (auto *spinBox : {ui->percentSpinBox, ui->widthSpinBox, ui->heightSpinBox}) {
        spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }

    // 按百分百
    ui->percentSpinBox->setRange(1, 200);
    ui->percentSpinBox->setValue(100);
    ui->percentSpinBox->setSuffix(" %");

    // 按长宽
    const int maximum = std::numeric_limits<int>::max();
    ui->widthSpinBox->setRange(1, maximum);
    ui->widthSpinBox->setSuffix(" px");
    ui->widthSpinBox->setValue(currentSize.width());

    ui->heightSpinBox->setRange(1, maximum);
    ui->heightSpinBox->setSuffix(" px");
    ui->heightSpinBox->setValue(currentSize.height());

    // 打开时用像素模式准确显示当前结果
    ui->pixelRadioButton->setChecked(true);

    // //预期高度
    // const int expectedHeight = toPixelCount( currentSize.width() * static_cast<double>(originalSize_.height()) / originalSize_.width());

    // 默认保存比例
    ui->keepAspectCheckBox->setChecked(true);

    ui->originalSizeLabel->setText(QString("原图尺寸：%1 × %2 px").arg(originalSize_.width()).arg(originalSize_.height()));
    ui->currentSizeLabel->setText(QString("当前尺寸：%1 × %2 px").arg(currentSize_.width()).arg(currentSize_.height()));
    ui->percentSpinBox->setToolTip("以本次打开对话框时的当前尺寸为基准");

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

    connect(ui->resetSizeButton, &QPushButton::clicked, this, &ResizeDialog::resetSize);
    connect(ui->restoreOriginalSizeButton, &QPushButton::clicked,
            this, &ResizeDialog::restoreOriginalSize);

    // 模板可能已连接按钮，这里统一配置，避免重复连接。
    disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    disconnect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateMode();
}

ResizeDialog::~ResizeDialog()
{
    delete ui;
}

QSize ResizeDialog::targetSize() const
{
    return QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
}

// 模式
void ResizeDialog::updateMode()
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
void ResizeDialog::updateFromPercent()
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
void ResizeDialog::updateFromWidth()
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
void ResizeDialog::updateFromHeight()
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

void ResizeDialog::updateSummary()
{
    const QSize size = targetSize();

    const qint64 pixels = static_cast<qint64>(size.width()) * size.height();

    //第一版暂定最多输出 4000 万像素。
    const bool allowed = pixels <= 40000000;

    QString text = QString("目标尺寸：%1 × %2 px").arg(size.width()).arg(size.height());

    if (!allowed)
    {
        text += "\n超过本版 4000 万像素的输出限制";
    }

    ui->targetSizeLabel->setText(text);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(allowed);
}

void ResizeDialog::resetSize()
{
    // 撤销对话框内的尺寸修改，回到打开对话框时的尺寸。
    setTargetSize(currentSize_);
}

void ResizeDialog::restoreOriginalSize()
{
    // 原图尺寸可能超出相对当前尺寸的百分比范围，使用像素模式。
    setTargetSize(originalSize_);
}

void ResizeDialog::setTargetSize(const QSize &size)
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

















