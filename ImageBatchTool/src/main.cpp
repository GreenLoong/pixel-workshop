#include "presentation/windows/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QIcon>
#include <QTimer>

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
    QApplication::setApplicationVersion(PIXEL_WORKSHOP_VERSION);
    QApplication::setOrganizationName("ImageBatchTool");
    QApplication::setWindowIcon(QIcon(":/app/icon.png"));

    QCommandLineParser arguments;
    arguments.addHelpOption();
    arguments.addVersionOption();
    arguments.addOption({"verify-startup", "Check window creation and shutdown without showing it."});
    arguments.process(app);
    applyPreferredFont();

    MainWindow window;
    const bool verifyStartup=arguments.isSet("verify-startup");
    if(verifyStartup)window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    if(verifyStartup) {
        QTimer::singleShot(0,&window,[&] {
            const bool created=window.testAttribute(Qt::WA_WState_Created) && window.winId()!=0;
            const bool closed=window.close();
            app.exit(created && closed?0:1);
        });
    }

    return app.exec();
}
