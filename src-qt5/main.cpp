#include <QTranslator>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QTextStream>
#include "mainUI.h"

inline void setupCursorTheme(QString theme){
  //Ensure dir existance but file deletion
  QString path = "/usr/local/share/icons/default/index.theme";
  if( QFile::exists(path)){ QFile::remove(path); }
  else{
    //Verify that the directory exists - or make it as needed
    QDir dir;
    dir.mkpath("/usr/local/share/icons/default");
  }
  //Assemble the file contents
  QStringList contents;
  contents << "[Icon Theme]";
  contents << "Name=Default";
  contents << "Comment=Default Cursor Theme";
  contents << "Inherits="+theme;
  //Now create the file
  QFile file(path);
  if(file.open(QIODevice::WriteOnly | QIODevice::Text)){
    QTextStream str(&file);
    str << contents.join("\n");
    str << "\n";
    file.close();
  }
}

int main(int argc, char ** argv)
{
    //Ensure the screens are setup properly (mirror all at highest common resolution)
    QProcess::execute("lumina-xconfig", QStringList() << "--mirror-monitors");

    //Setup the environment for this app
    // - Icon theme
    QIcon::setThemeName("le-trident");
    // - Cursor Theme
    setupCursorTheme("chameleon-pearl-regular");

    //Regular performance improvements
    qputenv("QT_NO_GLIB","1");
    //Create/start the application
    QApplication a(argc, argv);
    QFont font("noto-sans");
    a.setFont(font);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    MainUI w;

    return a.exec();
}
