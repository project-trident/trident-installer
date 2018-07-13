#include <QTranslator>
#include <QApplication>
#include <QDebug>
#include <QFile>

#include "mainUI.h"

int main(int argc, char ** argv)
{
    //Setup the environment for this app
    // - icon theme
    QIcon::setThemeName("le-trident");
    qputenv("QT_NO_GLIB","1");
    qputenv("QT_QPA_PLATFORMTHEME", "lthemeengine");
    //Create/start the application
    QApplication a(argc, argv);
    QFont font("noto-sans");
    a.setFont(font);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    MainUI w;

    return a.exec();
}
