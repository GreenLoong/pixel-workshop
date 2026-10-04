#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QIcon>

namespace
{

// 优先使用系统自带的中文字体，找不到时保留 Qt 默认字体。
void applyPreferredFont()
{
    const QStringList candidates = {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("PingFang SC"),
        QStringLiteral("Noto Sans CJK SC")
    };

    const QStringList families = QFontDatabase::families();

    for (const QString &family : candidates)
    {
        if (!families.contains(family))
            continue;

        QFont appFont = QApplication::font();
        appFont.setFamily(family);
        appFont.setPixelSize(13);
        QApplication::setFont(appFont);
        return;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 应用级样式只在入口设置一次。
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QApplication::setApplicationName("图像处理工具");
    QApplication::setApplicationDisplayName("图像处理工具");
    QApplication::setOrganizationName("ImageBatchTool");
    QApplication::setWindowIcon(QIcon(":/app/icon.png"));

    applyPreferredFont();

    MainWindow window;
    window.show();

    return app.exec();
}
