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
    explicit BrushPreview(QWidget *parent=nullptr);
    bool painting=false,foreground=true;
    double radius=0.025;
    std::function<void(ImageProcessor::BrushStroke)> onStroke;
protected:
    void paintEvent(QPaintEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    void append(QPoint point);
    ImageProcessor::BrushStroke stroke_;
    bool drawing_=false;
    QPoint pointer_;
    bool pointerInside_=false;
};
class BackgroundDialog final : public QDialog {
    Q_OBJECT
public:
    BackgroundDialog(const QImage &original,const ImageProcessor::Options &options,QWidget *parent=nullptr,
                     BrushPreview *sharedPreview=nullptr);
    ~BackgroundDialog() override;
    ImageProcessor::Options options() const {return working_;}
signals:
    void optionsChanged();
private:
    struct PreviewResult {QImage image,mask;QString error,notice;};
    void schedulePreview();
    void startPreview();
    void presentPreview();
    QImage base_;
    QImage result_;
    QImage mask_;
    ImageProcessor::Options working_;
    BrushPreview *preview_;
    SelectionItem *selection_;
    QDialogButtonBox *buttons_;
    QLabel *message_;
    QTimer debounce_;
    QFutureWatcher<PreviewResult> watcher_;
    bool dirty_=false;
    bool sharedPreview_;
    bool initializing_=true;
};
#endif
