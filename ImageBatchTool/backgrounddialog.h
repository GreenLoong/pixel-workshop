#ifndef BACKGROUNDDIALOG_H
#define BACKGROUNDDIALOG_H
#include "imageprocessor.h"
#include "previewlabel.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QImage>
#include <QTimer>
#include <functional>
class SelectionItem;
class QDialogButtonBox;
class QLabel;
class BrushPreview final : public PreviewLabel {
public:
    explicit BrushPreview(QWidget *parent=nullptr):PreviewLabel(parent){}
    bool painting=false,foreground=true;
    double radius=0.025;
    std::function<void(ImageProcessor::BrushStroke)> onStroke;
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    void append(QPoint point);
    ImageProcessor::BrushStroke stroke_;
    bool drawing_=false;
};
class BackgroundDialog final : public QDialog {
    Q_OBJECT
public:
    BackgroundDialog(const QImage &original,const ImageProcessor::Options &options,QWidget *parent=nullptr);
    ~BackgroundDialog() override;
    ImageProcessor::Options options() const {return working_;}
private:
    struct PreviewResult {QImage image;QString error;};
    void schedulePreview();
    void startPreview();
    QImage base_;
    ImageProcessor::Options working_;
    BrushPreview *preview_;
    SelectionItem *selection_;
    QDialogButtonBox *buttons_;
    QLabel *message_;
    QTimer debounce_;
    QFutureWatcher<PreviewResult> watcher_;
    bool dirty_=false;
};
#endif
