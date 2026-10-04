#ifndef GEOMETRYDIALOG_H
#define GEOMETRYDIALOG_H
#include "imageprocessor.h"
#include <QDialog>
#include <QImage>
class PreviewLabel;
class SelectionItem;
class QDoubleSpinBox;
class QLabel;
class QDialogButtonBox;
class QComboBox;
class GeometryDialog : public QDialog {
    Q_OBJECT
public:
    GeometryDialog(const QImage &original, const ImageProcessor::Options &options, QWidget *parent=nullptr,
                   PreviewLabel *sharedPreview=nullptr);
    ImageProcessor::Options options() const;
private:
    void updateImage();
    void updateSize();
    QImage previewSource_;
    QSize originalSize_;
    ImageProcessor::Options initial_;
    ImageProcessor::Options working_;
    PreviewLabel *preview_;
    SelectionItem *selection_;
    QDoubleSpinBox *angle_;
    QLabel *sizeLabel_;
    QDialogButtonBox *buttons_;
    QComboBox *ratio_;
    bool sharedPreview_;
};
#endif
