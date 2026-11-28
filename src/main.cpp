#include <QApplication>
#include <QCoreApplication>
#include "MainWindow.h"
#include "DiscordRPC.h"
#include "ConsoleMode.h"
#include "Version.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

static const int64_t DISCORD_CLIENT_ID = 1515443340865441892;

int main(int argc, char *argv[]) {
    if (ConsoleMode::wants(argc, argv)) {
#ifdef Q_OS_WIN
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
#endif
        QCoreApplication console(argc, argv);
        console.setApplicationName("FGDBuilder");
        console.setApplicationVersion(appVersion());
        return ConsoleMode::run(console.arguments());
    }

    QApplication app(argc, argv);
    app.setApplicationName("FGDBuilder");
    app.setApplicationVersion(appVersion());
    app.setOrganizationName("FGDBuilder");

    DiscordRPC::instance().init(DISCORD_CLIENT_ID);

    MainWindow window;
    window.show();

    int ret = app.exec();
    DiscordRPC::instance().shutdown();
    return ret;
}
