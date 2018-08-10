#include <QTranslator>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QTextStream>
#include <QDateTime>

#include "mainUI.h"

// THIS WILL NOT WORK ON AN ISO (read-only filesystem)
// === Add this as a static install file instead (files/index.theme)
/*inline void setupCursorTheme(QString theme){
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
}*/

QPalette colorPalette(){
  QPalette P;
  //Window Color
  P.setColor(QPalette::Active, QPalette::Window, QColor("#333333"));
  P.setColor(QPalette::Disabled, QPalette::Window, QColor("#333333"));
  P.setColor(QPalette::Inactive, QPalette::Window, QColor("#333333"));
  //WindowText Color
  P.setColor(QPalette::Active, QPalette::WindowText, QColor("#b3b3b3"));
  P.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#b3b3b3"));
  P.setColor(QPalette::Inactive, QPalette::WindowText, QColor("#b3b3b3"));
  //Base Color
  P.setColor(QPalette::Active, QPalette::Base, QColor("#b3b3b3"));
  P.setColor(QPalette::Disabled, QPalette::Base, QColor("#b3b3b3"));
  P.setColor(QPalette::Inactive, QPalette::Base, QColor("#b3b3b3"));
  //AlternateBase Color
  P.setColor(QPalette::Active, QPalette::AlternateBase, QColor("#333333"));
  P.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor("#333333"));
  P.setColor(QPalette::Inactive, QPalette::AlternateBase, QColor("#333333"));
  //ToolTipBase Color
  P.setColor(QPalette::Active, QPalette::ToolTipBase, QColor("#b3b3b3"));
  P.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor("#b3b3b3"));
  P.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor("#b3b3b3"));
  //ToolTipText Color
  P.setColor(QPalette::Active, QPalette::ToolTipText, QColor("black"));
  P.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor("black"));
  P.setColor(QPalette::Inactive, QPalette::ToolTipText, QColor("black"));
  //Text Color
  P.setColor(QPalette::Active, QPalette::Text, QColor("#333333"));
  P.setColor(QPalette::Disabled, QPalette::Text, QColor("#333333"));
  P.setColor(QPalette::Inactive, QPalette::Text, QColor("#333333"));
  //Button Color
  P.setColor(QPalette::Active, QPalette::Button, QColor("#b3b3b3"));
  P.setColor(QPalette::Disabled, QPalette::Button, QColor("#b3b3b3"));
  P.setColor(QPalette::Inactive, QPalette::Button, QColor("#b3b3b3"));
  //ButtonText Color
  P.setColor(QPalette::Active, QPalette::ButtonText, QColor("#333333"));
  P.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#a2a2a2"));
  P.setColor(QPalette::Inactive, QPalette::ButtonText, QColor("#333333"));
  //BrightText Color
  P.setColor(QPalette::Active, QPalette::BrightText, QColor("white"));
  P.setColor(QPalette::Disabled, QPalette::BrightText, QColor("white"));
  P.setColor(QPalette::Inactive, QPalette::BrightText, QColor("white"));
  //Highlight Color
  P.setColor(QPalette::Active, QPalette::Highlight, QColor("#1a59c3"));
  P.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#1a59c3"));
  P.setColor(QPalette::Inactive, QPalette::Highlight, QColor("#1a59c3"));
  //HighlightedText Color
  P.setColor(QPalette::Active, QPalette::HighlightedText, QColor("#b3b3b3"));
  P.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#b3b3b3"));
  P.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor("#b3b3b3"));
  //Link Color
  P.setColor(QPalette::Active, QPalette::Link, QColor("#1a59c3"));
  P.setColor(QPalette::Disabled, QPalette::Link, QColor("#1a59c3"));
  P.setColor(QPalette::Inactive, QPalette::Link, QColor("#1a59c3"));
  //LinkVisited Color
  P.setColor(QPalette::Active, QPalette::LinkVisited, QColor("#1a59c3"));
  P.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor("#1a59c3"));
  P.setColor(QPalette::Inactive, QPalette::LinkVisited, QColor("#1a59c3"));
  return P;
}

int main(int argc, char ** argv)
{
    //Ensure the screens are setup properly (mirror all at highest common resolution)
    if(QFile::exists("/dist")){
      //Only run this on the ISO
      QProcess::execute("lumina-xconfig", QStringList() << "--mirror-monitors");
    }

    //Setup the environment for this app
    // - Icon theme
    QStringList themes; themes << "le-trident" << "la-capitaine";
    for(int i=0; i<themes.length(); i++){
      if(QFile::exists("/usr/local/share/icons/"+themes[i])){
        QIcon::setThemeName(themes[i]); break;
      }
    }
    // - Cursor Theme
    //setupCursorTheme("chameleon-pearl-regular");

    //Regular performance improvements
    qputenv("QT_NO_GLIB","1");
    qunsetenv("QT_QPA_PLATFORMTHEME");
    //Create/start the application
    qsrand(QDateTime::currentMSecsSinceEpoch());
    QApplication a(argc, argv);
    QFont font("noto-sans");
    a.setFont(font);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);
    a.setPalette(colorPalette(),0); //default color palette for the app
    MainUI w;

    return a.exec();
}
