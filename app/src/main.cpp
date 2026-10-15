#include <QApplication>

#include "MainWindow.h"

using vcam::MainWindow;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Dynamsoft"));
    QCoreApplication::setApplicationName(QStringLiteral("VirtualCamera"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    MainWindow window;
    window.show();
    return app.exec();
}
