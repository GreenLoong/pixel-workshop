#pragma once
#include "domain/batchoptions.h"
#include <QWidget>
class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QStackedWidget;
class QLabel;
class QPushButton;
class BatchParameters final : public QWidget {
    Q_OBJECT
public:
    explicit BatchParameters(const ImageProcessor::Options &current,QWidget *parent=nullptr);
    BatchProcessing::Parameters parameters() const;
private:
    void load(const BatchProcessing::Parameters &parameters);
    void reloadPresets(const QString &selected={});
    void changed();
    BatchProcessing::Parameters draft_;
    QComboBox *preset_,*sizeMode_,*background_;
    QCheckBox *gray_,*upscale_;
    QSpinBox *width_,*height_,*longEdge_,*brightness_,*blur_,*feather_;
    QDoubleSpinBox *percent_,*contrast_;
    QStackedWidget *sizes_;
    QLabel *extra_;
    QPushButton *deletePreset_,*color_;
    bool loading_=false;
};
