//============================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//============================
#include "backend.h"

Backend::Backend(QObject *parent) : QObject(parent){

}

Backend::~Backend(){

}

inline QString confString(QString var, QString val){
  QString tmp = var.append("=\"%1\"");
  return tmp.arg(val);
}

QString Backend::generateInstallConfig(){
  // Turns JSON settings into a config file for pc-sysinstall
  QStringList contents, tmpL;
  QString tmp;
  // General install configuration
  contents << "installInteractive=no";

  // Localization
  contents << confString("localizeLang",lang());
  tmpL = keyboard();
  if(tmpL.length()>0){ contents << confString("localizeKeyLayout", tmpL[0]); }
  if(tmpL.length()>1){ contents << confString("localizeKeyModel", tmpL[1]); }
  if(tmpL.length()>2){ contents << confString("localizeKeyVariant", tmpL[2]); }

  // Timezone
  contents << confString("timeZone", timezone());
  contents << confString("enableNTP", useNTP() ? "yes" : "no");


  return contents.join("\n");
}

//Localization
QString Backend::lang(){
  QString tmp = settings.value("sys_lang").toString();
  if(tmp.isEmpty()){ tmp = "en_US.UTF-8"; }
  return tmp;
}

void Backend::setLang(QString lang){
  if(!lang.contains(".") && !lang.isEmpty()){ lang.append(".UTF-8"); }
  settings.insert("sys_lang", lang);
}

QStringList Backend::availableLanguages(){
  return QStringList();
}

//Keyboard Settings
QStringList Backend::keyboard(){
  //layout, model, variant
  QString tmp = settings.value("keyboard").toString();
  if(!tmp.isEmpty()){
    return tmp.split(", ");
  }
  return QStringList();
}

void Backend::setKeyboard(QStringList vals){
  //layout, model, variant
  settings.insert("keyboard",vals.join(", "));
}

QStringList Backend::availableKeyboards(QString layout, QString model){
  return QStringList();
}

//Time Settings
QString Backend::timezone(){
  QString tmp = settings.value("timezone").toString();
  if(!tmp.isEmpty()){ return tmp; }
  //Try to detect the currently-used timezone
  return QString(QTimeZone::systemTimeZoneId());
}

void Backend::setTimezone(QString tz){
  settings.insert("timezone", tz);
}

QDateTime Backend::localDateTime(){
  QDateTime cdt = QDateTime::currentDateTime();
  qDebug() << cdt;
  if(settings.contains("timezone")){ cdt = cdt.toTimeZone( QTimeZone( settings.value("timezone").toString().toUtf8() ) ); }
  qDebug() << cdt;
  if(settings.contains("time_offset_secs")){ cdt = cdt.addSecs( settings.value("time_offset_secs").toInt() ); }
  qDebug() << "Local Date Time:" << cdt;
  return cdt;
}

void Backend::setDateTime(QDateTime dt){
  //alternate to setTimezone() - sets both timezone and offset from system time
  //qDebug() << "Set Date Time:" << dt;
  QDateTime cdt = QDateTime::currentDateTime();
  settings.insert("time_offset_secs", cdt.secsTo(dt));
  settings.insert("timezone", QString(dt.timeZone().id()) );
}

QStringList Backend::availableTimezones(){
  QList<QByteArray> avail = QTimeZone::availableTimeZoneIds();
  QStringList list;
  for(int i=0; i<avail.length(); i++){ list << QString(avail[i]); }
  list.sort();
  return list;
}

bool Backend::useNTP(){
  QString tmp = settings.value("useNTP").toString();
  if(tmp.isEmpty()){ return true; } //default setting
  return (tmp.toLower()=="true");
}

void Backend::setUseNTP(bool use){
  settings.insert("useNTP", use ? "true" : "false" );
}
