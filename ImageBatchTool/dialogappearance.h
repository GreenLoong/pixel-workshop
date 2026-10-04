#ifndef DIALOGAPPEARANCE_H
#define DIALOGAPPEARANCE_H

#include <QList>
class QDialog;
class QDialogButtonBox;
class QWidget;

// 仅负责通用对话框外观和窗口移动，不接触图像或参数状态。
namespace DialogAppearance {
void setup(QDialog *dialog, const QList<QWidget *> &dragAreas);
void setupButtons(QDialogButtonBox *buttons);
}
#endif
