//============================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//============================
#include "backend.h"
#include <QtMath>

Backend::Backend(QObject *parent) : QObject(parent){

}

Backend::~Backend(){

}

QString Backend::runCommand(bool &success, QString command, QStringList arguments, QString workdir, QStringList env){
  QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels); //need output
  //First setup the process environment as necessary
  QProcessEnvironment PE = QProcessEnvironment::systemEnvironment();
    if(!env.isEmpty()){
      for(int i=0; i<env.length(); i++){
    if(!env[i].contains("=")){ continue; }
        PE.insert(env[i].section("=",0,0), env[i].section("=",1,100));
      }
    }
    proc.setProcessEnvironment(PE);
  //if a working directory is specified, check it and use it
  if(!workdir.isEmpty()){
    proc.setWorkingDirectory(workdir);
  }
  //Now run the command (with any optional arguments)
  if(arguments.isEmpty()){ proc.start(command); }
  else{ proc.start(command, arguments); }
  //Wait for the process to finish (but don't block the event loop)
  QString info;
  while(!proc.waitForFinished(1000)){
    if(proc.state() == QProcess::NotRunning){ break; } //somehow missed the finished signal
    QString tmp = proc.readAllStandardOutput();
    if(tmp.isEmpty()){ proc.terminate(); }
    else{ info.append(tmp); }
  }
  info.append(proc.readAllStandardOutput()); //make sure we don't miss anything in the output
  success = (proc.exitCode()==0); //return success/failure
  return info;
}


inline QString confString(QString var, QString val){
  QString tmp = var.append("=\"%1\"");
  return tmp.arg(val);
}

inline QString partitionDataToConf(QString devtag, partitiondata data){
  QString parts = data.create_partitions.join(",");
    if(parts.simplified().isEmpty()){ parts = "none"; }
  QString size = "0";
    if(data.sizeMB>0){ size = QString::number( qFloor(data.sizeMB) ); }
  QString line = QString("%1 %2 %3").arg(data.install_type, size, parts);
  if(!data.extrasetup.isEmpty()){ line.append( QString(" (%1)").arg(data.extrasetup) ); }
  return confString(devtag+"-part", line );
}

QString Backend::generateInstallConfig(){
  // Turns JSON settings into a config file for pc-sysinstall
  QStringList contents, tmpL;
  QString tmp;
  // General install configuration
  contents << "# == GENERAL OPTIONS ==";
  contents << "installInteractive=no";
  if(installToBE()){
    contents << confString("installMode", "upgrade");
    contents << confString("zpoolName", zpoolName() );
  }else{
    contents << confString("installMode", "fresh");
  }
  // Localization
  contents << "";
  contents << "# == LOCALIZATION ==";
  contents << confString("localizeLang",lang());
  tmpL = keyboard();
  if(tmpL.length()>0){ contents << confString("localizeKeyLayout", tmpL[0]); }
  if(tmpL.length()>1){ contents << confString("localizeKeyModel", tmpL[1]); }
  if(tmpL.length()>2){ contents << confString("localizeKeyVariant", tmpL[2]); }

  // Timezone
  contents << "";
  contents << "# == SYSTEM TIME ==";
  contents << confString("timeZone", timezone());
  contents << confString("enableNTP", useNTP() ? "yes" : "no");
  contents << confString("runCommand", QString("date -nu %1").arg( localDateTime().toUTC().toString("yyyyMMddHHmm") ) );

  // Networking
  contents << "";
  contents << "# == NETWORKING ==";
  contents << confString("netSaveDev", "AUTO-DHCP"); //DHCP for everything by default after install

  // Disks
  contents << "";
  contents << "# == DISK SETUP ==";
  for(int i=0; i<DISKS.length(); i++){
    diskdata DISK = DISKS[i];
    QString tag = "disk"+QString::number(i);
    contents << confString(tag, DISK.name);
    if(!DISK.mirror_disk.isEmpty()){
      contents << confString("mirror", DISK.mirror_disk);
    }
    if(DISK.install_partition.isEmpty() || DISK.install_partition.toLower()=="all" ){
      DISK.install_partition = "all";
      contents << confString("partscheme", "GPT"); //always use GPT partitioning (MBR is legacy/outdated)
    }
    contents << confString("partition", DISK.install_partition);
    contents << confString("bootManager", DISK.installBootManager ? "bsd" : "none");
    contents << "commitDiskPart";
    //Now assemble the partition layout lines
    for(int j=0; j<DISK.partitions.length(); j++){
      contents << partitionDataToConf(tag, DISK.partitions[j]);
    }
    contents << "commitDiskLabel";
  }

  // Users
  contents << "";
  contents << "# == USER ACCOUNTS ==";
  if(!rootPass().isEmpty()){ contents << confString("rootPass", rootPass()); }
  for(int i=0; i<USERS.length(); i++){
    userdata USER = USERS[i];
    contents << confString("userName", USER.name);
    contents << confString("userComment", USER.comment);
    contents << confString("userPass", USER.pass);
    contents << confString("userShell", USER.shell);
    contents << confString("userHome", USER.home);
    contents << confString("userGroups", USER.groups.join(","));
    if(USER.autologin){ contents << confString("autoLoginUser", USER.name); }
    contents << "commitUser";
  }
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

// USERS
QString Backend::rootPass(){ return settings.value("root_password").toString(); }
void Backend::setRootPass(QString pass){ settings.insert("root_password", pass); }

void Backend::addUser(userdata data){
  //will overwrite existing user with the same "name"
  for(int i=0; i<USERS.length(); i++){
    if(USERS[i].name == data.name){
      USERS.replace(i, data);
      return;
    }
  }
  USERS << data; //not found - add it to the end
}

void Backend::removeUser(QString name){
  for(int i=0; i<USERS.length(); i++){
    if(USERS[i].name == name){
      USERS.removeAt(i);
      return;
    }
  }
}
void Backend::clearUsers(){
  //remove all users
  USERS.clear();
}

//Disk Partitioning
QJsonObject Backend::availableDisks(){
  bool ok = false;
  QStringList disks = runCommand(ok, "pc-sysinstall disk-list").split("\n");
  QJsonObject obj;
  for(int i=0; i<disks.length() && ok; i++){
    if(disks[i].simplified().isEmpty()){ continue; }
    bool ok2 = false;
    QJsonObject diskObj;
    QString diskID = disks[i].section(":",0,0).simplified();
    QString diskInfo = disks[i].section(":",1,-1).simplified();
    QStringList info = runCommand(ok2, "pc-sysinstall", QStringList() << "disk-part" << diskID).split("\n");
    for(int j=0; j<info.length() && ok2; j++){
      if(info[j].simplified().isEmpty()){ continue; }
      QString var = info[j].section(": ",0,0);
      QString val = info[j].section(": ",1,-1);
      QString partition = var.section("-",0,0); var = var.section("-",1,-1);
      QJsonObject tmp = diskObj.value(partition).toObject();
      tmp.insert(var, val);
      diskObj.insert(partition, tmp);
    }
    if(ok2){
      QJsonObject tmp = diskObj.value(diskID).toObject();
        tmp.insert("label", diskInfo);
      diskObj.insert(diskID, tmp);
      obj.insert(diskID, diskObj);
    }
  } //end loop over disks
  return obj;
}

bool Backend::installToBE(){
  //will report true if a valid ZFS pool was designated
  QString tmp = settings.value("install_zpool").toString();
  return !tmp.isEmpty();
}

QString Backend::zpoolName(){
  //will return the designated ZFS pool name
  return settings.value("install_zpool").toString();
}

void Backend::setInstallToBE(QString pool){
  //set to an empty string to disable installing to a BE
  settings.insert("install_zpool", pool);
}

void Backend::addDisk(diskdata data){
  //will overwrite existing disk with the same name
  for(int i=0; i<DISKS.length(); i++){
    if(DISKS[i].name == data.name){
      DISKS.replace(i, data);
      return;
    }
  }
  DISKS << data; //not found - add it to the end
}

void Backend::removeDisk(QString name){
  for(int i=0; i<DISKS.length(); i++){
    if(DISKS[i].name == name){
      DISKS.removeAt(i);
      return;
    }
  }
}

void Backend::clearDisks(){
  DISKS.clear();
}
