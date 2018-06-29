//============================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//============================
#include "backend.h"
#include <QtMath>

// Set the bare-minimum partition install size for Trident
//  Current at 10GB
#define MIN_INSTALL_MB (10*1024)

Backend::Backend(QObject *parent) : QObject(parent){
  //Load the keyboard info cache in the background
  QtConcurrent::run(this, &Backend::checkKeyboardInfo);
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

QString Backend::mbToHuman(double mb){
  QStringList units; units << "MB" << "GB" << "TB" << "PB";
  int unit;
  for(unit=0; unit<units.length() && mb>1024; unit++){ mb = mb/1024; }
  //Round to 2 decimel places
  mb = qRound(mb*100)/100.0;
  return QString::number(mb)+units[unit];
}

QString Backend::readFile(QString path){
  QFile file(path);
  QString contents;
  if(file.open(QIODevice::ReadOnly)){
    QTextStream out(&file);
    contents = out.readAll();
    file.close();
  }
  return contents;
}

bool Backend::isLaptop(){
  static int hasbat = -1;
  if(hasbat<0){
    //Not probed yet
    bool ok = false;
    int ret = runCommand(ok, "apm -a").toInt();
    if(ret<0 || ret>2 || !ok){ hasbat = 0; } //no battery found
    else{ hasbat = 1; } //found a battery
  }
  return (hasbat==1);
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

void Backend::checkKeyboardInfo(){
  if(keyboardInfo.keys().isEmpty()){
    //Need to probe/cache the information about the available keyboards
    bool ok = false;
    QStringList models = runCommand(ok, "pc-sysinstall xkeyboard-models").replace("\t"," ").split("\n");
    QStringList layouts = runCommand(ok, "pc-sysinstall xkeyboard-layouts").replace("\t"," ").split("\n");
    QStringList variants = runCommand(ok, "pc-sysinstall xkeyboard-layouts").replace("\t"," ").split("\n");
    //Now put them into the cache
    QJsonObject modelObj;
    for(int i=0; i<models.length(); i++){
      if(models[i].simplified().isEmpty()){ continue; }
      modelObj.insert(models[i].section(" ",0,0).simplified(), models[i].section(" ",1,-1).simplified() ); //id : description
    }
    keyboardInfo.insert("models", modelObj);
    QJsonObject layObj;
    for(int i=0; i<layouts.length(); i++){
      if(layouts[i].simplified().isEmpty()){ continue; }
      QString id = layouts[i].section(" ",0,0).simplified();
      QString desc = layouts[i].section(" ",1,-1).simplified();
      QStringList layout_variants = variants.filter(" "+id+": ");
      QJsonObject var;
      for(int v=0; v<layout_variants.length(); v++){
        var.insert(layout_variants[i].section(" ",0,0), layout_variants[i].section(": ",1,-1) ); //id : description
      }
      QJsonObject obj;
        obj.insert("id", id);
        obj.insert("description", desc);
        obj.insert("variants", var);
      layObj.insert(id, obj);
    }
    keyboardInfo.insert("layouts", layObj);
    //qDebug() << "Got Keyboard info:" << keyboardInfo;
  }
}

void Backend::GeneratePackageItem(QJsonObject json, QTreeWidget *tree, QString name, QTreeWidgetItem *parent){
  if(json.isEmpty()){ return; }
  QTreeWidgetItem *item = 0;
  if(parent !=0){ item = new QTreeWidgetItem(parent); }
  else{ item = new QTreeWidgetItem(tree); }
  item->setText(0, name);
  if(json.contains("icon")){
    item->setIcon(0, QIcon::fromTheme(json.value("icon").toString()) );
  }
  if(json.contains("pkgname")){
    //Individual Package registration
    //item->setCheckable(0, true);
    item->setWhatsThis(0, json.value("pkgname").toString());
    bool setChecked = json.value("default").toBool(false) || (json.value("default_laptop").toBool(false) && isLaptop());
    //if(setChecked){ qDebug() << "Check Item:" << json << name; }
    item->setCheckState(0, setChecked ? Qt::Checked : Qt::Unchecked );
    if(json.value("required").toBool(false)){
      //item->setEnabled(0, false);
      item->setToolTip(0, tr("Required Package"));
    }
  }else{
    //Category entry
    QStringList list = json.keys();
    for(int i=0; i<list.length(); i++){
      GeneratePackageItem(json.value(list[i]).toObject(), tree, list[i], item);
    }
    item->setExpanded(true);
    //item->sortChildren(0, Qt::AscendingOrder);
  }
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

QJsonObject Backend::availableKeyboardModels(){
  checkKeyboardInfo();
  return keyboardInfo.value("models").toObject();
}

QJsonObject Backend::availableKeyboardLayouts(){
  checkKeyboardInfo();
  return keyboardInfo.value("layouts").toObject();
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
  //qDebug() << cdt;
  if(settings.contains("timezone")){ cdt = cdt.toTimeZone( QTimeZone( settings.value("timezone").toString().toUtf8() ) ); }
  //qDebug() << cdt;
  if(settings.contains("time_offset_secs")){ cdt = cdt.addSecs( settings.value("time_offset_secs").toInt() ); }
  //qDebug() << "Local Date Time:" << cdt;
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
    double totalMB = 0;
    for(int j=0; j<info.length() && ok2; j++){
      if(info[j].simplified().isEmpty()){ continue; }
      QString var = info[j].section(": ",0,0);
      QString val = info[j].section(": ",1,-1);
      QString partition = var.section("-",0,0); var = var.section("-",1,-1);
      if(var=="sizemb"){ totalMB+= val.toDouble(); }
      QJsonObject tmp = diskObj.value(partition).toObject();
      tmp.insert(var, val);
      diskObj.insert(partition, tmp);
    }
    if(ok2){
      //Add any extra info into the main diskID info object
      QJsonObject tmp = diskObj.value(diskID).toObject();
        tmp.insert("label", diskInfo);
        if(!tmp.contains("sizemb")){ tmp.insert("sizemb", QString::number( qRound(totalMB) ) ); }
      diskObj.insert(diskID, tmp);
      obj.insert(diskID, diskObj);
    }
  } //end loop over disks
  return obj;
}

QString Backend::diskInfoObjectToString(QJsonObject obj){
  QStringList tmp;
  QStringList keys = obj.keys();
  for(int i=0; i<keys.length(); i++){
    tmp << keys[i]+": "+obj.value(keys[i]).toString();
  }
  return tmp.join("\n");
}

QString Backend::diskInfoObjectToShortString(QJsonObject obj){
  QString tmp;
  if(obj.contains("label")){ tmp = obj.value("label").toString(); }
  else{ tmp = obj.value("sysid").toString(); }
  if(obj.contains("sizemb")){ tmp.append(", "+mbToHuman(obj.value("sizemb").toString().toDouble()) ); }
  return tmp;
}

bool Backend::checkValidSize(QJsonObject obj, bool installdrive, bool freespaceonly){
  double sizemb = 0;
  if(!freespaceonly && obj.contains("sizemb")){ sizemb = obj.value("sizemb").toString().toDouble(); } //partition size
  else if(obj.contains("freemb")){ sizemb = obj.value("freemb").toString().toDouble(); } //free space size
  double min = 0;
  if(installdrive){ min = MIN_INSTALL_MB; }
  else{ min = 1024; } //1GB bare minimum for things like swap and such
  return (sizemb >= min);
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

QStringList Backend::generateDefaultZFSPartitions(){
  QStringList parts;
  parts << "/(compress=lz4|atime=off)";
  parts << "/tmp(compress=lz4|setuid=off)";
  parts << "/usr(canmount=off|mountpoint=none)";
  parts << "/usr/home(compress=lz4)";
  parts << "/usr/jails(compress=lz4)";
  parts << "/usr/obj(compress=lz4)";
  parts << "/usr/ports(compress=lz4)";
  parts << "/usr/src(compress=lz4)";
  parts << "/var(canmount=off|atime=on|mountpoint=none)";
  parts << "/var/audit(compress=lz4)";
  parts << "/var/log(compress=lz4|exec=off|setuid=off)";
  parts << "/var/mail(compress=lz4)";
  parts << "/var/tmp(compress=lz4|exec=off|setuid=off)";

  return parts;
}

// == Packages ==
QStringList Backend::availableShells(){
  QStringList list;
  //Build-in shells
  list << "/bin/sh" << "/bin/csh" << "/bin/tcsh";
  //Now look for packaged shells
  // TODO
  return list;
}

QString Backend::defaultUserShell(){
  return "/bin/tcsh"; //change this to zsh later once the package files check is finished
}

void Backend::populatePackageTreeWidget(QTreeWidget *tree){
  QJsonObject pkgObj = QJsonDocument::fromJson( readFile(":/list/packages.json").toUtf8() ).object();
  //qDebug() << "Got Package Object:" << pkgObj;
  tree->clear();
  QStringList categories = pkgObj.keys();
  for(int i=0; i<categories.length(); i++){
    GeneratePackageItem(pkgObj.value(categories[i]).toObject(), tree,categories[i], 0); //top-level items
  }
}
