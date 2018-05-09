#include <QTranslator>
#include <QApplication>
#include <QDebug>
#include <QFile>

#include "mainUI.h"

int main(int argc, char ** argv)
{
    QApplication a(argc, argv);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);

    MainUI w;

    return a.exec();
}
