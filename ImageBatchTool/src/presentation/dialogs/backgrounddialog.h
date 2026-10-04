#ifndef BACKGROUNDDIALOG_H
#define BACKGROUNDDIALOG_H
#include "domain/imageprocessor.h"
#include "application/backgroundpreview.h"
#include "presentation/widgets/brushpreview.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QImage>
#include <QTimer>
class SelectionItem;
class QDialogButtonBox;
class QLabel;
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
    QFutureWatcher<BackgroundPreview::Result> watcher_;
    bool dirty_=false;
    bool sharedPreview_;
    bool initializing_=true;
};
#endif
