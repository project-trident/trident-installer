#include <QTranslator>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include "mainUI.h"

int main(int argc, char ** argv)
{
    //Ensure the screens are setup properly (mirror all at highest common resolution)
    QProcess::execute("lumina-xconfig", QStringList() << "--mirror-monitors");

    //Setup the environment for this app
    // - icon theme
    QIcon::setThemeName("le-trident");
    qputenv("QT_NO_GLIB","1");

    //Create/start the application
    QApplication a(argc, argv);
    QFont font("noto-sans");
    a.setFont(font);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    MainUI w;

    return a.exec();
}
