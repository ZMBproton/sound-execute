#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("sound_execute");
    QApplication::setApplicationDisplayName("sound.execute();");
    a.setWindowIcon(QIcon(":/icons/icon.svg"));
    QGuiApplication::setDesktopFileName("sound-execute");
    MainWindow w;
    w.show();
    return QApplication::exec();
}
