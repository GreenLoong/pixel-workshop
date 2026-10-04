#include "presentation/dialogs/resizedialog.h"
#include "ui_resizedialog.h"
#include "presentation/widgets/dialogappearance.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QIcon>
#include <QLayout>
#include <QBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QWidget>

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

    setupFramelessWindow();
    ui->closeButton->setText(QString());
    ui->closeButton->setIcon(QIcon(":/indicators/close.svg"));
    ui->closeButton->setIconSize(QSize(14, 14));

    DialogAppearance::setupButtons(ui->buttonBox);
    ui->resetSizeButton->setAutoDefault(false);
    ui->restoreOriginalSizeButton->setAutoDefault(false);

    // 无标题栏时用头部右上角的自绘按钮关闭对话框。
    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);

    // 保留键盘方向键调节，使用简洁的无箭头输入框。
    for (auto *spinBox : {ui->percentSpinBox, ui->widthSpinBox, ui->heightSpinBox}) {
        spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }

    // 按百分比
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

    // .ui 中没有按钮盒连接，统一在这里连接一次。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 摘要区按最坏情况锁定高度：
    // 尺寸文字可能换行，超限时还会再多一行提示；预留三行后，
    // 是否显示提示都不会改变卡片高度，避免控件错位。
    {
        const QFontMetrics metrics(ui->currentSizeLabel->font());
        const int lineHeight = metrics.lineSpacing();

        ui->currentSizeLabel->setFixedHeight(lineHeight);
        ui->targetSizeLabel->setFixedHeight(lineHeight * 3);
        ui->summaryPanel->setFixedHeight(2 * 16 + metrics.height() + 10 + lineHeight * 3);
    }

    // 窗口只按布局的最小高度自适应，宽度可由用户拖动改变。
    layout()->setSizeConstraint(QLayout::SetMinimumSize);

    // 保证摘要区两列都有足够宽度，尺寸文字不会换行到第三行。
    setMinimumWidth(640);

    updateMode();

    // 按当前布局把窗口调整到最小宽度，保证初始宽度就是设计宽度。
    resize(qMax(640, sizeHint().width()), sizeHint().height());
}

ResizeDialog::~ResizeDialog()
{
    delete ui;
}
void ResizeDialog::embedInEditor()
{
    DialogAppearance::embed(this);
    ui->comparisonLayout->setDirection(QBoxLayout::TopToBottom);
    ui->arrowLabel->hide();
    ui->summaryPanel->setFixedHeight(175);
    ui->resetLayout->setDirection(QBoxLayout::TopToBottom);
    while(auto *item=ui->inputLayout->takeAt(0))delete item;
    const QList<QWidget *> labels{ui->percentSpinBoxLabel,ui->widthSpinBoxLabel,ui->heightSpinBoxLabel};
    const QList<QWidget *> inputs{ui->percentSpinBox,ui->widthSpinBox,ui->heightSpinBox};
    for(int i=0;i<3;++i){ui->inputLayout->addWidget(labels[i],i,0);ui->inputLayout->addWidget(inputs[i],i,1);}
    ui->contentLayout->setContentsMargins(10,12,10,12);
}

// 去掉系统标题栏，窗口外观完全由样式表决定。
void ResizeDialog::setupFramelessWindow()
{
    ui->rootLayout->setContentsMargins(12, 12, 12, 12);
    DialogAppearance::setup(this, {ui->headerPanel, ui->dialogHeading, ui->dialogSubtitle});
    ui->closeButton->setCursor(Qt::ArrowCursor);
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

    // 第一版暂定最多输出 4000 万像素。
    const bool allowed = pixels <= 40000000;

    const QString sizeText = QString("目标尺寸：%1 × %2 px").arg(size.width()).arg(size.height());

    // 提示按超限与否在两行内切换；标签高度固定，因此不会改变对话框高度。
    ui->targetSizeLabel->setText(allowed
                                     ? sizeText
                                     : sizeText + "\n超过本版 4000 万像素的输出限制");

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(allowed);
    if(size!=lastTargetSize_) {
        lastTargetSize_=size;
        emit targetSizeChanged(size);
    }

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
