#ifndef EDITORPAGE_H
#define EDITORPAGE_H
#include "domain/imageprocessor.h"
#include <QWidget>
#include <QImage>
#include <QTimer>
#include <vector>
class QVBoxLayout;
class QButtonGroup;
class QPushButton;
class QLabel;
class BrushPreview;

// 编辑会话只持有参数草稿；原图、保存、全局历史由主窗口管理。
class EditorPage final : public QWidget {
    Q_OBJECT
public:
    enum Mode { Crop, Tone, Resize, Background };
    explicit EditorPage(QWidget *parent=nullptr);
    ~EditorPage() override;
    void begin(const QImage &original,const ImageProcessor::Options &options,const QString &name,Mode mode=Crop);
    ImageProcessor::Options options() const;
    void selectMode(Mode mode);
    void undo();
    void redo();
    void end();
signals:
    void accepted();
    void cancelled();
    void draftChanged();
private:
    bool eventFilter(QObject *,QEvent *) override;
    void buildPanel(bool reusePreview=false);
    void record();
    void updateButtons();
    bool valid() const;
    QImage original_;
    ImageProcessor::Options draft_;
    Mode mode_=Crop;
    QWidget *panel_=nullptr;
    bool resizePreviewPending_=false;
    QWidget *host_;
    QVBoxLayout *body_;
    BrushPreview *preview_;
    QButtonGroup *modes_;
    QPushButton *undo_,*redo_,*done_;
    QLabel *name_,*zoom_;
    QTimer recordTimer_;
    struct State {ImageProcessor::Options options;Mode mode;};
    std::vector<State> history_;
    int index_=0;
};
#endif
