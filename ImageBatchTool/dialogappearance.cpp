#include "dialogappearance.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QLayout>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QWindow>

namespace {
class DialogDragFilter final : public QObject
{
public:
    explicit DialogDragFilter(QDialog *dialog) : QObject(dialog), dialog_(dialog) {}
protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (dialog_->isWindow() && event->type() == QEvent::MouseButtonPress) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && dialog_->windowHandle()
                && dialog_->windowHandle()->startSystemMove())
                return true;
        }
        return QObject::eventFilter(watched, event);
    }
private:
    QDialog *dialog_;
};
}

void DialogAppearance::embed(QDialog *dialog)
{
    dialog->setWindowFlags(Qt::Widget);
    dialog->setAttribute(Qt::WA_TranslucentBackground,false);
    dialog->setGraphicsEffect(nullptr);
    dialog->setMinimumSize(0,0);
    dialog->setStyleSheet(QString()); // 统一继承编辑页主题。
    dialog->layout()->setContentsMargins(0,0,0,0);
    dialog->layout()->setSizeConstraint(QLayout::SetDefaultConstraint);
    for(const char *name:{"headerPanel","dialogHeading","dialogSubtitle","closeButton","footerPanel","modeHint"})
        if(auto *widget=dialog->findChild<QWidget *>(name))widget->hide();
    if(auto *content=dialog->findChild<QWidget *>("contentPanel"))
        content->layout()->setContentsMargins(0,0,0,0);
    for(auto *button:dialog->findChildren<QPushButton *>()) {
        button->setDefault(false);button->setAutoDefault(false);
    }
}

void DialogAppearance::setup(QDialog *dialog, const QList<QWidget *> &dragAreas)
{
    dialog->setWindowFlag(Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_TranslucentBackground);
    QFile style(":/styles/dialog.qss");
    if (style.open(QIODevice::ReadOnly))
        dialog->setStyleSheet(QString::fromUtf8(style.readAll()));
    auto *shadow = new QGraphicsDropShadowEffect(dialog);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(15, 23, 42, 70));
    dialog->setGraphicsEffect(shadow);
    auto *filter = new DialogDragFilter(dialog);
    for (auto *area : dragAreas) {
        area->setCursor(Qt::SizeAllCursor);
        area->installEventFilter(filter);
    }
}

void DialogAppearance::setupButtons(QDialogButtonBox *buttons)
{
    auto *confirm = buttons->button(QDialogButtonBox::Ok);
    auto *cancel = buttons->button(QDialogButtonBox::Cancel);
    confirm->setObjectName("confirmButton");
    confirm->style()->unpolish(confirm);
    confirm->style()->polish(confirm);
    confirm->setText("确定");
    cancel->setText("取消");
    confirm->setDefault(true);
    confirm->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *layout = buttons->layout();
    for (int i = 0; i < layout->count(); ++i) {
        if (auto *spacer = layout->itemAt(i)->spacerItem())
            spacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    layout->invalidate();
}
