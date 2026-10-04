#include "tonedialog.h"
#include "dialogappearance.h"
#include "imageprocessing.h"
#include "previewlabel.h"
#include "sliderstyle.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <exception>

ToneDialog::ToneDialog(const QImage &original, const ImageProcessor::Options &options,
                       QWidget *parent)
    : QDialog(parent), initial_(options),
      preview_(new PreviewLabel(this)), brightness_(new QSpinBox(this)),
      contrast_(new QDoubleSpinBox(this)), buttons_(new QDialogButtonBox(
          QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)),
      error_(new QLabel(this))
{
    setObjectName("ToneDialog");
    setWindowTitle("亮度与对比度");
    resize(760, 650);
    setMinimumSize(640, 600);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(0);
    auto *content = new QWidget(this);
    content->setObjectName("contentPanel");
    auto *body = new QVBoxLayout(content);
    body->setContentsMargins(24, 20, 24, 20);
    body->setSpacing(16);
    root->addWidget(content, 1);

    auto *header = new QWidget(content);
    auto *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *heading = new QLabel("亮度与对比度", header);
    heading->setObjectName("dialogHeading");
    auto *subtitle = new QLabel("按住此处可以拖动窗口", header);
    subtitle->setObjectName("dialogSubtitle");
    titles->addWidget(heading);
    titles->addWidget(subtitle);
    headerRow->addLayout(titles, 1);
    auto *close = new QToolButton(header);
    close->setObjectName("closeButton");
    close->setIcon(QIcon(":/indicators/close.svg"));
    close->setIconSize(QSize(14, 14));
    close->setToolTip("关闭（Esc）");
    close->setFixedSize(32, 32);
    headerRow->addWidget(close);
    body->addWidget(header);
    auto *divider = new QFrame(content);
    divider->setObjectName("divider");
    divider->setFrameShape(QFrame::HLine);
    body->addWidget(divider);
    auto *hint = new QLabel("拖动滑块即时预览，点击轨道直接定位。确定后应用，取消保留当前图片。", content);
    hint->setObjectName("modeHint");
    hint->setWordWrap(true);
    body->addWidget(hint);
    preview_->setObjectName("tonePreview");
    body->addWidget(preview_, 1);

    brightness_->setObjectName("brightnessSpinBox");
    brightness_->setRange(-100, 100);
    brightness_->setValue(options.brightness);
    brightness_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    brightness_->setFixedWidth(100);
    contrast_->setObjectName("contrastSpinBox");
    contrast_->setRange(0.5, 2.0);
    contrast_->setDecimals(2);
    contrast_->setSingleStep(0.05);
    contrast_->setValue(options.contrast);
    contrast_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    contrast_->setFixedWidth(100);
    auto *brightnessSlider = new QSlider(Qt::Horizontal, this);
    brightnessSlider->setObjectName("brightnessSlider");
    brightnessSlider->setRange(-100, 100);
    brightnessSlider->setValue(options.brightness);
    auto *contrastSlider = new QSlider(Qt::Horizontal, this);
    contrastSlider->setObjectName("contrastSlider");
    contrastSlider->setRange(50, 200);
    contrastSlider->setValue(qRound(options.contrast * 100));
    for (auto *slider : {brightnessSlider, contrastSlider})
        AbsoluteSliderStyle::applyTo(slider);
    const auto addRow = [this, body](const QString &text, QSlider *slider, QWidget *input) {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(text, this);
        label->setFixedWidth(64);
        row->setSpacing(14);
        row->addWidget(label);
        row->addWidget(slider, 1);
        row->addWidget(input);
        body->addLayout(row);
    };
    addRow("亮度", brightnessSlider, brightness_);
    addRow("对比度", contrastSlider, contrast_);
    error_->setWordWrap(true);
    error_->setStyleSheet("color: #d92d20;");
    error_->hide();
    body->addWidget(error_);
    auto *reset = new QPushButton("重置亮度和对比度", this);
    reset->setObjectName("resetToneButton");
    reset->setAutoDefault(false);
    body->addWidget(reset, 0, Qt::AlignLeft);
    auto *footer = new QWidget(this);
    footer->setObjectName("footerPanel");
    auto *footerRow = new QHBoxLayout(footer);
    footerRow->setContentsMargins(24, 16, 24, 16);
    footerRow->addWidget(buttons_);
    root->addWidget(footer);
    DialogAppearance::setup(this, {header, heading, subtitle});
    DialogAppearance::setupButtons(buttons_);
    close->setCursor(Qt::ArrowCursor);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(brightnessSlider, &QSlider::valueChanged, brightness_, &QSpinBox::setValue);
    connect(brightness_, &QSpinBox::valueChanged, this, [this, brightnessSlider](int value) {
        const QSignalBlocker blocker(brightnessSlider);
        brightnessSlider->setValue(value);
        updatePreview();
    });
    connect(contrastSlider, &QSlider::valueChanged, this, [this](int value) {
        contrast_->setValue(value / 100.0);
    });
    connect(contrast_, &QDoubleSpinBox::valueChanged, this, [this, contrastSlider](double value) {
        const QSignalBlocker blocker(contrastSlider);
        contrastSlider->setValue(qRound(value * 100));
        updatePreview();
    });
    connect(reset, &QPushButton::clicked, this, [this, brightnessSlider, contrastSlider] {
        const QSignalBlocker blockBrightness(brightness_);
        const QSignalBlocker blockContrast(contrast_);
        brightness_->setValue(0);
        contrast_->setValue(1.0);
        // 信号被阻断后主动同步两个滑块，预览只计算一次。
        const QSignalBlocker blockBrightnessSlider(brightnessSlider);
        const QSignalBlocker blockContrastSlider(contrastSlider);
        brightnessSlider->setValue(0);
        contrastSlider->setValue(100);
        updatePreview();
    });
    try {
        // 灰度和尺寸在本对话框中不变，先生成中性预览，调节时只重算亮度和对比度。
        // 限制预览长边，避免每次拖动都处理原图；确认时主窗口仍使用完整原图。
        const QSize actualSize = options.targetSize == cv::Size()
            ? original.size() : QSize(options.targetSize.width, options.targetSize.height);
        const QSize previewSize = actualSize.scaled(QSize(1280, 1280), Qt::KeepAspectRatio)
            .boundedTo(actualSize);
        auto baseOptions = options;
        baseOptions.brightness = 0;
        baseOptions.contrast = 1.0;
        baseOptions.targetSize = cv::Size(previewSize.width(), previewSize.height());
        const QImage source = original.width() > 1280 || original.height() > 1280
            ? original.scaled(1280, 1280, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : original;
        basePreview_ = ImageProcessing::processImage(source, baseOptions);
        updatePreview();
    } catch (const std::exception &error) {
        error_->setText("预览失败：" + QString::fromUtf8(error.what()));
        error_->show();
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}

ImageProcessor::Options ToneDialog::options() const
{
    auto result = initial_;
    result.brightness = brightness_->value();
    result.contrast = contrast_->value();
    return result;
}

void ToneDialog::updatePreview()
{
    if (basePreview_.isNull())
        return;
    try {
        ImageProcessor::Options tone;
        tone.brightness = brightness_->value();
        tone.contrast = contrast_->value();
        preview_->setImage(QPixmap::fromImage(ImageProcessing::processImage(basePreview_, tone)));
        error_->hide();
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(true);
    } catch (const std::exception &error) {
        error_->setText("预览失败：" + QString::fromUtf8(error.what()));
        error_->show();
        buttons_->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
}
