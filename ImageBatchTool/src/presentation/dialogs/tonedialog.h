#ifndef TONEDIALOG_H
#define TONEDIALOG_H

#include "application/imagetask.h"
#include <QDialog>
#include <QImage>

class PreviewLabel;
class QSpinBox;
class QDoubleSpinBox;
class QDialogButtonBox;
class QLabel;

// 对话框只管理参数编辑和自己的预览，不修改主窗口状态。
class ToneDialog : public QDialog
{
    Q_OBJECT
public:
    ToneDialog(const QImage &original, const ImageProcessor::Options &options,
               QWidget *parent = nullptr,PreviewLabel *sharedPreview=nullptr);
    ImageProcessor::Options options() const;

private:
    void updatePreview();
    ImageTask task_;
    QImage basePreview_;
    ImageProcessor::Options working_;
    PreviewLabel *preview_;
    QSpinBox *brightness_;
    QDoubleSpinBox *contrast_;
    QDialogButtonBox *buttons_;
    QLabel *error_;
};
#endif
