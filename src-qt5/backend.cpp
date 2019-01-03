//============================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//============================
#include "backend.h"
#include <QtMath>

// Set the bare-minimum partition install size for Trident
//  Current at 5GB
#define MIN_INSTALL_MB (5*1024)

Backend::Backend(QObject *parent) : QObject(parent){
  PROC = new QProcess(this);
  connect(PROC, SIGNAL(readyRead()), this, SLOT(read_install_output()) );
  connect(PROC, SIGNAL(started()), this, SIGNAL(install_started()) );
  connect(PROC, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(install_finished(int, QProcess::ExitStatus)) );

  //Load the keyboard info cache in the background
  QtConcurrent::run(this, &Backend::checkKeyboardInfo);
  //Load the disk info cache in the background
  QtConcurrent::run(this, &Backend::availableDisks, true);
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

bool Backend::writeFile(QString path, QString contents){
  QFile file(path);
  if( !file.open(QIODevice::WriteOnly | QIODevice::Truncate) ){ return false; }
  QTextStream in(&file);
  in << contents;
  file.close();
  return true;
}

bool Backend::isLaptop(){
  static int hasbat = -1;
  if(hasbat<0){
    //Not probed yet
    bool ok = false;
    QStringList info = runCommand(ok, "devinfo").split("\n").filter("acpi_acad0");
    if( info.isEmpty() || !ok){ hasbat = 0; } //no battery found
    else{ hasbat = 1; } //found a battery
  }
  return (hasbat==1);
}

inline QString confString(QString var, QString val, bool needquotes = false){
  QString tmp=var.append("=%1");
  if(needquotes){ tmp = tmp.arg("\""+val+"\""); }
  else{ tmp = tmp.arg(val); }
  return tmp;
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

QString Backend::system_information(){
  //Note: Return this in HTML format (better text formatting in UI)
  checkPciConf();
  bool ok = false;
  QStringList info;
  info << "<b><u>Overview</u></b>";
  info << QString("<b>Boot Method:</b> %1").arg( isUEFI() ? "UEFI" : "Legacy");
  info << QString("<b>System Type:</b> %1").arg( isLaptop() ? "Laptop" : "Workstation");
  info << QString("<b>System Model:</b> %1").arg( runCommand(ok, "sysctl", QStringList()<< "-hn" << "hw.model") );
  info << QString("<b>System Architecture:</b> %1").arg( runCommand(ok, "sysctl", QStringList()<< "-hn" << "hw.machine") );
  info << "";
  info << "<b><u>Basic Information</u></b>";
  info << QString("<b>Number of CPUs:</b> %1").arg( runCommand(ok, "sysctl", QStringList()<< "-hn" << "hw.ncpu").remove("\n") );
  info << QString("<b>Real Memory:</b> %1").arg( bytesToHuman(runCommand(ok, "sysctl", QStringList()<< "-n" << "hw.realmem")) );
  info << QString("<b>Physical Memory:</b> %1").arg( bytesToHuman(runCommand(ok, "sysctl", QStringList()<< "-n" << "hw.physmem")) );
  info << "";
  info << "<b><u>Graphics Device Info</u></b>";
  QStringList gpuList = pciconf.keys().filter("vgapci");
  for(int i=0; i<gpuList.length(); i++){
    info << QString("<b>GPU %1 Vendor:</b> %2").arg( QString::number(i+1), pciconf.value(gpuList[i]).toObject().value("vendor").toString() );
    info << QString("<b>GPU %1 Device:</b> %2").arg( QString::number(i+1), pciconf.value(gpuList[i]).toObject().value("device").toString() );
  }
  QStringList netList = runCommand(ok, "ifconfig -l").split(" ",QString::SkipEmptyParts);
  if(!netList.isEmpty()){
    info << "";
    info << "<b><u>Network Device Info</u></b>";
    for(int i=0; i<netList.length(); i++){
      if(!pciconf.contains(netList[i])){ continue; }
      info << QString("<b>%1 Vendor:</b> %2").arg( netList[i], pciconf.value(netList[i]).toObject().value("vendor").toString() );
      info << QString("<b>%1 Device:</b> %2").arg( netList[i], pciconf.value(netList[i]).toObject().value("device").toString() );
    }
  }
  netList = runCommand(ok, "sysctl -n net.wlan.devices").split(" ",QString::SkipEmptyParts);
  if(!netList.isEmpty()){
    bool first = true;
    for(int i=0; i<netList.length(); i++){
      if(!pciconf.contains(netList[i])){ continue; }
      if(first){ //first valid wifi device - go ahead and generate the header/label
        info << "";
        info << "<b><u>Wireless Device Info</u></b>";
        first = false;
      }
      info << QString("<b>%1 Vendor:</b> %2").arg( netList[i], pciconf.value(netList[i]).toObject().value("vendor").toString() );
      info << QString("<b>%1 Device:</b> %2").arg( netList[i], pciconf.value(netList[i]).toObject().value("device").toString() );
    }
  }
  return info.join("<br>");
}

QString Backend::pci_info(){
  checkPciConf();
  return QJsonDocument(pciconf).toJson(QJsonDocument::Indented);
}

bool Backend::isUEFI(){
  static int chk = -1;
  if(chk<0){
    bool ok = false;
    QString result = runCommand(ok, "sysctl", QStringList()<< "-n" << "machdep.bootmethod").simplified().toLower();
    if(ok){
      //qDebug() << "Got UEFI Check:" << result;
      if(result=="uefi"){ chk = 0; } //booting with UEFI
      else{ chk = 1; } //booting with legacy mode
    }
  }
  return (chk==0);
}

QString Backend::isodate(){
  bool ok = false;
  QJsonObject config = QJsonDocument::fromJson( readFile("/var/db/trueos-manifest.json").toLocal8Bit() ).object();
  return config.value("os_version").toString();
}

QString Backend::generateInstallConfig(bool internal){
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
    //Now setup the custom ZFS pool name as needed
    contents << confString("zpoolName", customPoolName());
  }
  if(use_4k_alignment()){
    contents << confString("zfsForce4k","YES");
    contents << confString("ashift","12");
  }
  contents << confString("efiloader","bsd"); //can also be "refind"
  contents << confString("hostname", hostname() );

  //Settings for loading files from the ISO
  contents << "";
  contents << "# == INSTALL FROM ISO ==";
  contents << confString("packageType","pkg");
  contents << confString("installType","FreeBSD");
  contents << confString("installMedium","local");
  contents << confString("localPath", getLocalDistPath());
  contents << confString("distFiles","base doc kernel lib32");

  // Localization
  contents << "";
  contents << "# == LOCALIZATION ==";
  contents << confString("localizeLang",lang());
  tmpL = keyboard(); //this always returns a list of length 3 (layout, model, variant)
  if(!tmpL[0].isEmpty()){ contents << confString("localizeKeyLayout", tmpL[0]); }
  if(!tmpL[1].isEmpty()){ contents << confString("localizeKeyModel", tmpL[1]); }
  if(!tmpL[2].isEmpty()){ contents << confString("localizeKeyVariant", tmpL[2]); }

  // Timezone
  contents << "";
  contents << "# == SYSTEM TIME ==";
  contents << confString("timeZone", timezone());
  contents << confString("enableNTP", useNTP() ? "yes" : "no");
  contents << confString("runCommand", QString("date -nu %1").arg( localDateTime().toUTC().toString("yyyyMMddHHmm") ) );

  // Networking
  //contents << "";
  //contents << "# == NETWORKING ==";
  //contents << confString("netSaveDev", "AUTO-DHCP"); //DHCP for everything by default after install

  // Disks
  if(!installToBE()){
    //Only create this section if NOT installing to a boot environment
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
        //Full-disk install
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
  }

  // Users
  contents << "";
  contents << "# == USER ACCOUNTS ==";
  if(!rootPass().isEmpty()){ contents << confString("rootPass", (internal ? rootPass() : "******") ); }
  for(int i=0; i<USERS.length(); i++){
    userdata USER = USERS[i];
    contents << confString("userName", USER.name);
    contents << confString("userComment", USER.comment);
    contents << confString("userPass", (internal ? USER.pass : "******") );
    contents << confString("userShell", USER.shell);
    contents << confString("userHome", USER.home);
    contents << confString("userGroups", USER.groups.join(","));
    if(USER.autologin){ contents << confString("autoLoginUser", USER.name); }
    contents << "commitUser";
  }

  // Packages
  if(!settings.value("install_packages").isNull() ){
    contents << "";
    contents << "# == PACKAGES ==";
    contents << confString("installPackages", settings.value("install_packages").toString() );
  }
  //Additional Setup
  contents << "";
  contents << "# == FINAL SETUP ==";
  contents << confString("runCommand", "/usr/local/share/trident/scripts/sys-init.sh");
  contents << confString("runCommand", "touch /usr/local/etc/trident/.firstboot");
  if(installToBE()){
    contents << confString("runExtCommand", "/usr/local/bin/activateNewBE "+zpoolName());
  }
  contents << ""; //always end the config with an empty line - otherwise the last instruction will not be used
  return contents.join("\n");
}

QString Backend::getLocalDistPath(){
  static QString cachedir;
  if(cachedir.isEmpty()){
    QDir dir("/dist");
    QStringList list = dir.entryList(QStringList() << "FreeBSD*", QDir::Dirs | QDir::NoDotAndDotDot);
    for(int i=0; i<list.length(); i++){
      if(dir.exists(list[i]+"/latest")){ cachedir = dir.absoluteFilePath(list[i]+"/latest"); break; }
    }
    if(cachedir.isEmpty()){ cachedir = dir.absolutePath(); } //no FreeBSD subdir? - should never happen but try using just /dist
  }
  return cachedir;
}

void Backend::checkKeyboardInfo(){
  bool loaded = false;
  bool ok = false;
  if(!keyboardInfo.contains("models")){
    //Need to probe/cache the information about the available keyboards
    QStringList models = runCommand(ok, "pc-sysinstall xkeyboard-models").replace("\t"," ").split("\n");
    //Now put them into the cache
    QJsonObject modelObj;
    for(int i=0; i<models.length(); i++){
      if(models[i].simplified().isEmpty()){ continue; }
      modelObj.insert(models[i].section(" ",0,0).simplified(), models[i].section(" ",1,-1).simplified() ); //id : description
    }
    if(!modelObj.isEmpty()){
      keyboardInfo.insert("models", modelObj);
      loaded = true;
    }
  }
  if(!keyboardInfo.contains("layouts")){
    QJsonObject layObj;
    QStringList layouts = runCommand(ok, "pc-sysinstall xkeyboard-layouts").replace("\t"," ").split("\n");
    QStringList variants = runCommand(ok, "pc-sysinstall xkeyboard-variants").replace("\t"," ").split("\n");
    //Now put it into the cache
    for(int i=0; i<layouts.length(); i++){
      if(layouts[i].simplified().isEmpty()){ continue; }
      QString id = layouts[i].section(" ",0,0).simplified();
      QString desc = layouts[i].section(" ",1,-1).simplified();
      QStringList layout_variants = variants.filter(" "+id+": ");
      QJsonObject var;
      for(int v=0; v<layout_variants.length(); v++){
        var.insert(layout_variants[v].section(" ",0,0), layout_variants[v].section(": ",1,-1) ); //id : description
      }
      QJsonObject obj;
        obj.insert("id", id);
        obj.insert("description", desc);
        obj.insert("variants", var);
      layObj.insert(id, obj);
    }
    if(!layObj.isEmpty()){
      keyboardInfo.insert("layouts", layObj);
      loaded = true;
    }
    //qDebug() << "Got Keyboard info:" << keyboardInfo;
  }
  if(loaded){ emit keyboardInfoAvailable(); }
}

void Backend::checkPciConf(){
  if(pciconf.isEmpty()){
    bool ok = false;
    QStringList info = runCommand(ok, "pciconf -lv").replace("\t"," ").split("\n");
    QJsonObject tmp; QString id;
    for(int i=0; i<info.length(); i++){
      //Check for a top-level item line
      if(info[i].contains("@") && info[i].contains("rev=")){
        if(!id.isEmpty()){ pciconf.insert(id, tmp); }
        tmp = QJsonObject(); //clear this
        id = info[i].section("@",0,0).simplified();
        tmp.insert("location", info[i].section(" ",0,0).section("@",-1).simplified() );
        //ignore the rest of the hex-code information for top-level item lines
      }else{
        QString value = info[i].section("=",1,-1).simplified();
        if(value.startsWith("\'") && value.endsWith("\'")){ value.chop(1); value.remove(0,1); }
        tmp.insert( info[i].section("=",0,0).simplified(), value);
      }
    }
    //Make sure we insert the last item into the object too
    if(!id.isEmpty()){ pciconf.insert(id, tmp); }
    //qDebug() << "Got PCI Conf:" << pciconf;
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
    item->setWhatsThis(0, json.value("pkgname").toString());
    QJsonObject infoObj = package_info(item->whatsThis(0));
    if(infoObj.isEmpty()){
      qDebug() << "Package not found:" << item->whatsThis(0);
      delete item;
      return;
    }
    //qDebug() << "Got package info:" << infoObj;
    item->setText(1, infoObj.value("version").toString().section(",",0,0));
    item->setText(2, infoObj.value("comment").toString().simplified());
    bool setChecked = json.value("default").toBool(false) || (json.value("default_laptop").toBool(false) && isLaptop());
    if(!setChecked && json.contains("pciconf_match") ){
      checkPciConf();
      QJsonObject conf = json.value("pciconf_match").toObject();
      QStringList ids = pciconf.keys();
      if(conf.contains("id_regex")){ ids = ids.filter(QRegExp(conf.value("id_regex").toString())); }
      for(int i=0; i<ids.length() && !setChecked; i++){
        QJsonObject info = pciconf.value(ids[i]).toObject();
        bool match = true;
        if(conf.contains("vendor_regex")){ match = match && info.value("vendor").toString().contains(QRegExp(conf.value("vendor_regex").toString())); }
        if(conf.contains("device_regex")){ match = match && info.value("device").toString().contains(QRegExp(conf.value("device_regex").toString())); }
        if(conf.contains("class_regex")){ match = match && info.value("class").toString().contains(QRegExp(conf.value("class_regex").toString())); }
        if(conf.contains("subclass_regex")){ match = match && info.value("subclass").toString().contains(QRegExp(conf.value("subclass_regex").toString())); }
        setChecked = match;
      }
    }
    //if(setChecked){ qDebug() << "Check Item:" << json << name; }
    item->setCheckState(0, setChecked ? Qt::Checked : Qt::Unchecked );
    if(json.value("required").toBool(false)){
      item->setDisabled(true);
      item->setToolTip(0, tr("Required Package"));
    }
  }else{
    //Category entry
    QStringList list = json.keys();
    for(int i=0; i<list.length(); i++){
      GeneratePackageItem(json.value(list[i]).toObject(), tree, list[i], item);
    }
    if(json.value("expand").toBool(true)){ item->setExpanded(true); }
    if(json.value("single_selection").toBool(false)){
      item->setData(0, 100, "single");
      item->setText(0, item->text(0)+" ("+tr("limit 1")+")");
    }
    item->sortChildren(0, Qt::AscendingOrder);
    item->setFirstColumnSpanned(true);
    if(item->childCount()<1){ item->setHidden(true); }
  }
}

QString Backend::bytesToHuman(QString bytes){
  QStringList units; units << "B" << "KB" << "MB" << "GB" << "TB" << "PB";
  int unit = 0;
  double bts = bytes.toDouble();
  while(bts>1000 && unit<units.length()){
    unit++;
    bts = bts/1024.0;
  }
  //Round off remaining number to 2 decimel places
  bts = qRound(bts*100.0)/100.0;
  return (QString::number(bts)+units[unit] );
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
  //Probe the list of available translation files for this info
  QStringList langs;
  langs << "en_US.UTF-8"; //this is always available (default)
  QDir _dir("/usr/local/share/trident-installer/i18n");
  QStringList files = _dir.entryList(QStringList()<<"tri-install_*.qm", QDir::Files, QDir::Name);
  for(int i=0; i<files.length(); i++){
    langs << files[i].section("_",1,-1).section(".qm",0,-2)+".UTF-8";
  }
  return langs;
}

//Keyboard Settings
QStringList Backend::keyboard(){
  //pre-fill the output list
  QStringList list; list << "" << "" << "";
  //layout, model, variant
  QString tmp = settings.value("keyboard").toString();
  if(!tmp.isEmpty()){
    list = tmp.split(", ");
  }else{
    //Need to probe the system to get the info
    bool ok = false;
    QStringList info = runCommand(ok, "setxkbmap -query").split("\n");
    if(ok){
      for(int i=0; i<info.length(); i++){
        if(info[i].startsWith("layout:")){ list[0] = info[i].section(":",-1).simplified(); }
        else if(info[i].startsWith("model:")){ list[1] = info[i].section(":",-1).simplified(); }
        else if(info[i].startsWith("variant:")){ list[2] = info[i].section(":",-1).simplified(); }
      }
    }
  }
  while(list.length()<3){ list << ""; }
  return list;
}

void Backend::setKeyboard(QStringList vals, bool changenow){
  while(vals.length()<3){ vals << ""; }
  //layout, model, variant
  settings.insert("keyboard",vals.join(", "));
  if(changenow){
    bool ok = false;
    QStringList args;
    if(!vals[0].isEmpty()){ args << "-layout" << vals[0]; }
    if(!vals[1].isEmpty()){ args << "-model" << vals[1]; }
    if(!vals[2].isEmpty()){ args << "-variant" << vals[2]; }
    runCommand(ok, "setxkbmap", args);
  }
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
  if(settings.contains("timezone")){ cdt.setTimeZone( QTimeZone( settings.value("timezone").toString().toUtf8() ) ); }
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

//Hostname
QString Backend::hostname(){
  if(!settings.contains("system_hostname")){
    //Generate a random hostname (trident-XXXX)
    setHostname("trident-" + QString::number( qrand() %8999 + 1000 ) );
  }
  return settings.value("system_hostname").toString();
}

void Backend::setHostname(QString name){
  //Provide user-input validation here (TO-DO)
  settings.insert("system_hostname", name);
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
QJsonObject Backend::availableDisks(bool fromcache){
  static QJsonObject diskObj;
  if(diskObj.isEmpty() || !fromcache){
    bool ok = false;
    QStringList disks = runCommand(ok, "pc-sysinstall disk-list").split("\n");
    QJsonObject obj;
    for(int i=0; i<disks.length() && ok; i++){
      if(disks[i].simplified().isEmpty() ){ continue; }
      bool ok2 = false;
      QJsonObject dObj;
      QString diskID = disks[i].section(":",0,0).simplified();
      if( !QFile::exists("/dev/"+diskID) ){ continue; }
      QString diskInfo = disks[i].section(":",1,-1).simplified();
      QStringList info = runCommand(ok2, "pc-sysinstall", QStringList() << "disk-part" << diskID).split("\n");
      double totalMB = 0;
      for(int j=0; j<info.length() && ok2; j++){
        if(info[j].simplified().isEmpty()){ continue; }
        QString var = info[j].section(": ",0,0);
        QString val = info[j].section(": ",1,-1);
        QString partition = var.section("-",0,0); var = var.section("-",1,-1);
        if(var=="sizemb"){ totalMB+= val.toDouble(); }
        QJsonObject tmp = dObj.value(partition).toObject();
        tmp.insert(var, val);
        dObj.insert(partition, tmp);
      }
      if(ok2){
        //Add any extra info into the main diskID info object
        QJsonObject tmp = dObj.value(diskID).toObject();
          tmp.insert("label", diskInfo);
          if(tmp.contains("freemb")){ totalMB+=tmp.value("freemb").toDouble(); }
          tmp.insert("sizemb", QString::number( qRound(totalMB) ) );
        dObj.insert(diskID, tmp);
        obj.insert(diskID, dObj);
      }
    } //end loop over disks
    diskObj = obj;
  }
  return diskObj;
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

QStringList Backend::availableZPools(){
  //Format: "name :: description"
  QStringList pools;
  bool ok = false;
  QStringList tmp = runCommand(ok, "zpool import").split("\n").filter("pool: ");
  //qDebug() << "Got ZFS pool list:" << tmp;
  for(int i=0; i<tmp.length(); i++){
    QString pname = tmp[i].section(" ",1,1,QString::SectionSkipEmpty);
    pools << pname+" :: "+tmp[i].section(" ", 1,-1, QString::SectionSkipEmpty);
  }
  return pools;
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

// Custom ZPool name
QString Backend::customPoolName(){
  QString name = settings.value("new_zpool_name").toString();
  if(name.isEmpty()){
    name="trident";
  }
  return name.simplified();
}

void Backend::setCustomPoolName(QString name){
  settings.insert("new_zpool_name", name);
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

// - 4K Disk alignment
bool Backend::use_4k_alignment(){
  //enabled by default
  bool set = true;
  if(settings.contains("force_4k_alignment")){
    set = settings.value("force_4k_alignment").toBool(true);
  }
  return set;
}

void Backend::set4k_alignment(bool set){
  settings.insert("force_4k_alignment", set);
}

// Boot Manager Installations
bool Backend::BM_refindAvailable(){
  return (QFile::exists(REFIND_ZIP) && isUEFI() );
}

bool Backend::install_refind(){
  return (0 == QProcess::execute("/usr/local/bin/install-refind") );

}

// == Packages ==
QString Backend::dist_package_dir(){
  static QString dist_dir = "";
  if(dist_dir.isEmpty()){
    //Find the directory with the distribution package files
    QString base = "/dist"; //Dir on the ISO
    if(!QFile::exists(base)){ base = "/usr/local/pkg-cache"; } //Local Dir for testing
    QDir _dir(base);
    QStringList dirs = _dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    //Note - this dir is several levels deep with variable dir names - but ultimately is a single dir -> single dir format.
    while(!dirs.isEmpty()){
      //Go another level deep
      _dir.cd(dirs.first());
      dirs = _dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    }
    dist_dir = _dir.absolutePath();
  }
  return dist_dir;
}

QJsonObject Backend::package_info(QString pkgname){
  QDir _dir(dist_package_dir());
  QStringList files = _dir.entryList(QStringList() << pkgname+"-*.txz", QDir::Files | QDir::NoSymLinks, QDir::Name);
  if(files.isEmpty()){ return QJsonObject(); } //no info available - package not found?
  QJsonObject obj;
  for(int i=0; i<files.length() && obj.isEmpty(); i++){
    QString filepath = _dir.absoluteFilePath(files[i]);
    bool ok = false;
    QStringList info = runCommand(ok, "pkg", QStringList() << "query" << "-F" << filepath << "%o|%n|%v|%c|%e").split("|");
    if(!ok || info.length()<5 || info[1]!=pkgname ){ continue; }
    obj.insert("origin", info[0]);
    obj.insert("name", info[1] );
    obj.insert("version", info[2] );
    obj.insert("comment", info[3] );
    obj.insert("description", info[4] );
  }
  qDebug() << "Could not read Package info:" << pkgname;
  return obj;
}

QStringList Backend::availableShells(QTreeWidget *pkgtree){
  QStringList list;
  //Build-in shells
  list << "/bin/sh" << "/bin/csh" << "/bin/tcsh";
  //Now look for packaged shells
  QStringList search; search << "zsh" << "fish" << "bash";
  for(int i=0; i<search.length(); i++){
   QList<QTreeWidgetItem*> items = pkgtree->findItems(search[i], Qt::MatchFixedString | Qt::MatchRecursive,0);
   if( !items.isEmpty() && (items.first()->checkState(0)==Qt::Checked) ){ list << "/usr/local/bin/"+search[i]; }
  }
  //Now sort the shells into alphabetical order
  for(int i=0; i<list.length(); i++){
    list[i] = list[i].section("/",-1)+":"+list[i];
  }
  list.sort();
  for(int i=0; i<list.length(); i++){
    list[i] = list[i].section(":",-1);
  }
  return list;
}

QStringList Backend::defaultUserShell(){
  QStringList defs; defs << "/usr/local/bin/zsh" << "/usr/local/bin/fish" << "/usr/local/bin/bash" << "/bin/tcsh";
  return defs;
}

void Backend::populatePackageTreeWidget(QTreeWidget *tree){
  QJsonObject pkgObj = QJsonDocument::fromJson( readFile(":/list/packages.json").toUtf8() ).object();
  //qDebug() << "Got Package Dist Dir:" << dist_package_dir();
  //qDebug() << "Got Package Object:" << pkgObj;
  tree->clear();
  QStringList categories = pkgObj.keys();
  for(int i=0; i<categories.length(); i++){
    GeneratePackageItem(pkgObj.value(categories[i]).toObject(), tree,categories[i], 0); //top-level items
  }
  tree->resizeColumnToContents(0);
}

void Backend::setInstallPackages(QStringList pkgs){
  //Note that these are package names - not port origins
  settings.insert("install_packages", pkgs.join(" "));
}

void Backend::setInstallPackages(QTreeWidget *tree){
  QStringList list;
  list << "trident-core"; //always required
  QTreeWidgetItemIterator it(tree);
  while(*it){
    if( (*it)->checkState(0) == Qt::Checked && !(*it)->whatsThis(0).isEmpty() ){
      list << (*it)->whatsThis(0);
    }
   ++it;
  }
  setInstallPackages(list);
}

void Backend::read_install_output(){
  QString txt = PROC->readAll();
  if(!txt.isEmpty()){
    emit install_update(txt);
  }
}

void Backend::install_finished(int retcode, QProcess::ExitStatus status){
  read_install_output();
  //qDebug() << "Installation Finished:" <<retcode << status;
  emit install_finished(retcode==0);
}

// == PUBLIC SLOTS ==
void Backend::startInstallation(){
  //Save the config file
  QString configfile = INSTALLCONF;
  bool ok = writeFile(configfile, generateInstallConfig() );
  if(!ok){
    emit install_update("[ERROR] Cannot create installation config file: "+configfile );
    emit install_finished(false);
    return;
  }
  //Start the install process
  PROC->start("pc-sysinstall", QStringList() << "-c" << configfile );
}
