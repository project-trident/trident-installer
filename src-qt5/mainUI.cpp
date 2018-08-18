//===========================================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//===========================================
#include "mainUI.h"
#include "ui_mainUI.h"
#include <QDebug>
#include <QLocale>
#include <QScrollBar>
#include <QScreen>

#include <unistd.h>

MainUI::MainUI() : QMainWindow(), ui(new Ui::MainUI){
  ui->setupUi(this); //load the designer file
  BACKEND = new Backend(this);
  connect(BACKEND, SIGNAL(keyboardInfoAvailable()), this, SLOT(populateKeyboardInfo()) );
  connect(BACKEND, SIGNAL(install_update(QString)), this, SLOT(newInstallMessage(QString)) );
  //connect(BACKEND, SIGNAL(install_started()), this, SLOT(newInstallMessage(QString)) );
  connect(BACKEND, SIGNAL(install_finished(bool)), this, SLOT(installFinished()) );
  ui->actionKeyboard->setEnabled(false);
  DEBUG = (getuid()!=0) || !QFile::exists("/dist/");
  //PAGE ORDER
  page_list << ui->page_welcome << ui->page_partitions << ui->page_pkgs << ui->page_user << ui->page_summary;
  //NOTE: page_installing and page_finished are always at the end of the list;

  //Now setup any other UI elements
  ui->progress_pages->setRange(0,page_list.count());
  ui->stackedWidget->setCurrentWidget(page_list.first());
  loadPageFromBackend(page_list.first());
  //Create internal timers/objects
  slideshowTimer = new QTimer(this);
    slideshowTimer->setInterval(5000);

  //Only load the package page once
  BACKEND->populatePackageTreeWidget(ui->tree_pkgs);

  //Setup the sidebar/buttons
  last_sidebar_size = 0;
  sidebar_group = new QActionGroup(this);
    sidebar_group->setExclusive(true);
    sidebar_group->addAction(ui->actionInfo);
    sidebar_group->addAction(ui->actionKeyboard);
    sidebar_group->addAction(ui->actionLocale);
    sidebar_group->addAction(ui->actionLog);
  QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  ui->toolBar->insertWidget(ui->actionLog, spacer); //put a spacer between the log and the other items
  collapse_sidebar();
  //ui->toolBar->widgetForAction(ui->actionInfo)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  //ui->toolBar->widgetForAction(ui->actionKeyboard)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  //ui->toolBar->widgetForAction(ui->actionLocale)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  //Update the visuals and show the window
  setupConnections();
  updateButtonFrame();
  ui->tabWidget_disks->setCurrentIndex(0);
  if(DEBUG){
    this->show();
  }else{
    QRect size = QApplication::screens().first()->geometry();
    this->setGeometry(size);
    this->showMaximized();
  }
  this->show();
  ui->label_debug->setVisible(DEBUG);
  //BACKEND->availableKeyboardModels();
  // Disable all the UI elements that are not finished yet
  ui->group_disk_refind->setVisible(false);
  ui->radio_disk_mirror->setVisible(false);

  //Setup the default SWAP size based on amount of memory in the system
  int swapsize = 1024; //MB
  //Default size adjustment TO-DO
  swap_size_changed(swapsize);
}

MainUI::~MainUI(){

}

void MainUI::setupConnections(){
  //Sidebar items
  connect(sidebar_group, SIGNAL(triggered(QAction*)), this, SLOT(sidebar_item_changed()) );
  connect(ui->tool_collapse_sidebar, SIGNAL(clicked()), this, SLOT(collapse_sidebar()) );
  connect(ui->splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(sidebar_size_changed()) );
  connect(ui->list_locale, SIGNAL(currentRowChanged(int)), this, SLOT(localeChanged()) );
  //Button Bar items
  connect(ui->tool_next, SIGNAL(clicked()), this, SLOT(nextClicked()) );
  connect(ui->tool_prev, SIGNAL(clicked()), this, SLOT(prevClicked()) );
  connect(ui->tool_startinstall, SIGNAL(clicked()), this, SLOT(startInstallClicked()) );
  connect(ui->tool_reboot, SIGNAL(clicked()), this, SLOT(rebootClicked()) );
  connect(ui->tool_shutdown, SIGNAL(clicked()), this, SLOT(shutdownClicked()) );
  connect(slideshowTimer, SIGNAL(timeout()), this, SLOT(nextSlideshowImage()) );
  connect(ui->tabWidget, SIGNAL(tabBarClicked(int)), slideshowTimer, SLOT(stop()) );
  //Welcome Page
  connect(ui->dateTimeEdit, SIGNAL(dateTimeChanged(const QDateTime&)), this, SLOT(userDT_changed()) );
  connect(ui->combo_welcome_timezone, SIGNAL(currentTextChanged(const QString&)), this, SLOT(userDT_changed()) );
  //User Page
  connect(ui->line_pass_root, SIGNAL(textEdited(const QString&)), this, SLOT(validateUserPage()) );
  connect(ui->line_passrepeat_root, SIGNAL(textEdited(const QString&)), this, SLOT(validateUserPage()) );
  connect(ui->tool_showpass_root, SIGNAL(toggled(bool)), this, SLOT(validateUserPage()) );
  connect(ui->line_user_pass, SIGNAL(textEdited(const QString&)), this, SLOT(validateUserPage()) );
  connect(ui->line_user_pass2, SIGNAL(textEdited(const QString&)), this, SLOT(validateUserPage()) );
  connect(ui->tool_user_showpass, SIGNAL(toggled(bool)), this, SLOT(validateUserPage()) );
  connect(ui->line_user_name, SIGNAL(textEdited(const QString&)), this, SLOT(validateUserPage()) );
  connect(ui->line_user_comment, SIGNAL(textEdited(const QString&)), this, SLOT(validateUserPage()) );
  connect(ui->line_user_comment, SIGNAL(editingFinished()), this, SLOT(autogenerateUsername()) );
  //Package Page
  connect(ui->tree_pkgs, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(pkg_item_changed(QTreeWidgetItem*,int)) );
  //Disk Page
  connect(ui->radio_disk_single, SIGNAL(toggled(bool)), this, SLOT(radio_disk_toggled()) );
  connect(ui->radio_disk_bootenv, SIGNAL(toggled(bool)), this, SLOT(radio_disk_toggled()) );
  connect(ui->radio_disk_mirror, SIGNAL(toggled(bool)), this, SLOT(radio_disk_toggled()) );
  connect(ui->slider_disk_swap, SIGNAL(valueChanged(int)), this, SLOT(swap_size_changed(int)) );
  connect(ui->line_disk_pass, SIGNAL(textChanged(QString)), this, SLOT(validateDiskPage()) );
  connect(ui->line_disk_pass_chk, SIGNAL(textChanged(QString)), this, SLOT(validateDiskPage()) );
  connect(ui->group_disk_encrypt, SIGNAL(toggled(bool)), this, SLOT(validateDiskPage()) );
  connect(ui->tree_disks, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(validateDiskPage()) );

}

// ===============
//    PUBLIC FUNCTIONS
// ===============


//==============
//  PRIVATE
//==============
void MainUI::loadPageFromBackend(QWidget *current){
  //Note: This will never run for the installation/finished pages
  if(current == ui->page_welcome){
    //qDebug() << "Load Welcome Page Settings";
    if(ui->combo_welcome_timezone->count()==0){
      ui->combo_welcome_timezone->addItems(BACKEND->availableTimezones());
    }
    QDateTime dt = BACKEND->localDateTime();
    int index = ui->combo_welcome_timezone->findText( dt.timeZone().id() );
    if(index>=0){ ui->combo_welcome_timezone->setCurrentIndex(index); }
    ui->dateTimeEdit->setDateTime(dt);

  }else if(current == ui->page_partitions){
    QJsonObject obj = BACKEND->availableDisks();
    ui->tree_disks->clear();
    //qDebug() << "Got Disks:" << obj;
    QStringList disks = obj.keys();
    for(int i=0; i<disks.length(); i++){
      QJsonObject info = obj.value(disks[i]).toObject();
      QTreeWidgetItem *disk = new QTreeWidgetItem(ui->tree_disks, QStringList() << disks[i]+" ("+Backend::mbToHuman(info.value(disks[i]).toObject().value("sizemb").toString().toDouble())+")");
        disk->setIcon(0, QIcon::fromTheme(disks[i].startsWith("da") ? "media-removable" : "harddrive"));
        disk->setToolTip(0, BACKEND->diskInfoObjectToString(info.value(disks[i]).toObject()) );
        disk->setWhatsThis(0, disks[i] + " : all"); //disk ID, partition type
        disk->setData(0, Qt::UserRole, info.value(disks[i]).toObject().value("sizemb").toString());
        disk->setExpanded(true);
      QStringList parts = info.keys();
      for(int p=0; p<parts.length(); p++){
        QJsonObject pinfo = info.value(parts[p]).toObject();
        if(disks[i] == parts[p]){
           disk->setToolTip( 0, BACKEND->diskInfoObjectToString(pinfo) );
           //Need to create the treewidgetitem for the freespace too (freemb)
           QTreeWidgetItem *part = new QTreeWidgetItem(disk, QStringList() << "Free Space ("+Backend::mbToHuman(pinfo.value("freemb").toString().toDouble())+")");
             part->setWhatsThis(0, disks[i]+" : free");
             part->setToolTip(0, tr("Unpartitioned free space"));
             part->setDisabled(!BACKEND->checkValidSize(pinfo, true, true) );
             part->setData(0, Qt::UserRole, pinfo.value("freemb").toString());
        }else{
          QTreeWidgetItem *part = new QTreeWidgetItem(disk, QStringList() << parts[p]+" ("+BACKEND->diskInfoObjectToShortString(pinfo)+")" );
            part->setWhatsThis(0, disks[i] + " : " + parts[p].remove(disks[i]) ); //disk ID, partition type
            part->setToolTip( 0, BACKEND->diskInfoObjectToString(pinfo) );
            part->setDisabled(!BACKEND->checkValidSize(pinfo, true) );
            part->setData(0, Qt::UserRole, pinfo.value("sizemb").toString());
        }
      }
    }
    if(ui->tree_disks->currentItem()==0){
      ui->tree_disks->setCurrentItem( ui->tree_disks->topLevelItem(0) ); //first item
    }
    //ZPOOL List (Boot Environment options)
    disks = BACKEND->availableZPools();
    ui->list_zpools->clear();
    QString current = BACKEND->zpoolName();
    for(int i=0; i<disks.length(); i++){
      ui->list_zpools->addItem(disks[i].section(" :: ", 1,-1));
      ui->list_zpools->item(i)->setWhatsThis(disks[i].section(" :: ", 0,0));
      if(disks[i].startsWith(current+" :: ")){ ui->list_zpools->setCurrentRow(i); }
    }
    if(ui->list_zpools->currentRow()<0 && !disks.isEmpty()){ ui->list_zpools->setCurrentRow(0); }
    //Now see if the boot environment option is even available
    if(disks.isEmpty()){
      ui->radio_disk_bootenv->setEnabled(false);
      ui->radio_disk_bootenv->setChecked(false);
    }
    radio_disk_toggled(); //Make sure the page items are updated appropriately

  }else if(current == ui->page_user){
    //Need to fill the list of available shells
    QStringList shells = BACKEND->availableShells(ui->tree_pkgs);
    QString defshell = ui->combo_user_shell->currentText();
    if(defshell.isEmpty() || shells.filter("/bin/"+defshell).isEmpty()){
      //Now figure out the new default shell (first time, or shell no longer getting installed)
      defshell.clear();
      QStringList defaultshell = BACKEND->defaultUserShell(); //priority-ordered
      for(int i=0; i<defaultshell.length() && defshell.isEmpty(); i++){
        if(shells.contains(defaultshell[i])){ defshell = defaultshell[i]; }
      }
      //qDebug() << "Got Shells:" << shells << defshell;
    }
    //Add the shells to the list
    ui->combo_user_shell->clear();
    for(int i=0; i<shells.length(); i++){
      ui->combo_user_shell->addItem( shells[i].section("/",-1), shells[i]);
      if(defshell.section("/",-1) == shells[i].section("/",-1) ){ ui->combo_user_shell->setCurrentIndex(i); }
    }
    //Load the info from the backend
    ui->line_pass_root->setText(BACKEND->rootPass());
    QList<userdata> users = BACKEND->users();
    if(users.count() > 0){
      ui->line_user_name->setText(users[0].name);
      ui->line_user_comment->setText(users[0].comment);
      ui->line_user_pass->setText(users[0].pass);
      ui->line_user_name->setText(users[0].name);
    }
    validateUserPage();

  }else if(current == ui->page_summary){
    ui->text_summary->clear();
    ui->text_summary->setPlainText( BACKEND->generateSummary() );

  }

}

bool MainUI::savePageToBackend(QWidget *current, bool prompts){
  //Note: This will never run for the installation/finished pages
  if(current == ui->page_user){
    BACKEND->clearUsers();
    userdata data;
      data.name = ui->line_user_name->text();
      data.comment = ui->line_user_comment->text();
      data.pass = ui->line_user_pass->text();
      data.shell = ui->combo_user_shell->currentData().toString();
      data.groups << "wheel" << "operator";
      data.home = "/usr/home/"+data.name;
      data.autologin = false;
    BACKEND->addUser(data);
    BACKEND->setRootPass(ui->line_pass_root->text());

  }else if(current == ui->page_partitions){
    if(ui->radio_disk_bootenv->isChecked()){
      BACKEND->setInstallToBE( ui->list_zpools->currentItem()->whatsThis() );
    }else{
      //Single-disk setup
      BACKEND->setInstallToBE(""); //disable this
      if(ui->tree_disks->currentItem()==0){ return false; }
      QString sel = ui->tree_disks->currentItem()->whatsThis(0);
      double totalMB = ui->tree_disks->currentItem()->data(0, Qt::UserRole).toString().toDouble();
      if(BACKEND->disks().isEmpty() && sel.endsWith("all") && prompts){
        //Full disk selected. Verify that this is what the user intends
        bool ok = (QMessageBox::Yes == QMessageBox::warning(this, tr("Verify Hard Disk"), QString(tr("This will destroy any existing partitions and all data on the selected hard drive. Are you sure you want to continue? ")+"\n\n %1").arg(sel.section(":",0,0).simplified()), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes ) );
        if(!ok){ return false; }
      }
      //Create the disk/partition entry for the backend
      BACKEND->clearDisks();
      diskdata disk;
        disk.name = sel.section(":",0,0).simplified();
        disk.install_partition = sel.section(":",1,1).simplified();
        disk.mirror_disk.clear(); //unset this
        disk.installBootManager = (disk.install_partition=="all");
      //Setup partitions for the disk
      if(disk.install_partition=="all" && ui->group_disk_swap->isChecked()){
        double swapsize = ui->slider_disk_swap->value();
        //create a SWAP partition on full-disk installs
        partitiondata swap;
          swap.install_type = "SWAP.eli"; //make sure we use encrypted swap by default
          swap.sizeMB = swapsize; //1GB swap by default
        disk.partitions << swap;
        totalMB-=swapsize; //decrease the total size a bit to account for the swap partition
      }
      //Now setup the main partition for ZFS
      partitiondata part;
        if(ui->group_disk_encrypt->isChecked()){
          part.install_type = "ZFS.eli";
          part.encrypt_pass = ui->line_disk_pass->text();
        }else{
          part.install_type = "ZFS";
          part.encrypt_pass = "";
        }
        part.sizeMB = totalMB - 10; //Make it 10MB smaller than calculated to account for rounding errors and such
        part.create_partitions  = BACKEND->generateDefaultZFSPartitions();
      disk.partitions.prepend(part); //make sure this partition is always first
      //Now save it into the backend
      BACKEND->addDisk(disk);
    } //end single-disk setup

  }else if(current == ui->page_pkgs){
    BACKEND->setInstallPackages(ui->tree_pkgs);
  }
  return true;
}

//==============
//  PRIVATE SLOTS
//==============
void MainUI::collapse_sidebar(){
  int width = ui->centralwidget->width();
  ui->splitter->setSizes(QList<int>() << 0 << width);
  ui->splitter->handle(1)->setEnabled(false);
  QAction *checked = sidebar_group->checkedAction();
  if(checked!=0){ checked->setChecked(false); }
}

void MainUI::sidebar_item_changed(){
  //Determine which item is checked and adjust sidebar as needed
  bool opensidebar = (ui->splitter->sizes().first()==0);
  //Figure out which page in the sidebar should be shown
  QAction *checked = sidebar_group->checkedAction();
  if(checked == ui->actionLocale){
    ui->stacked_sidebar->setCurrentWidget(ui->page_locale);
    if(ui->list_locale->count()<1){
      QString current = BACKEND->lang();
      QStringList langs = BACKEND->availableLanguages();
      langs.sort();
      for(int i=0; i<langs.length(); i++){
        QLocale locale(langs[i]);
        ui->list_locale->addItem("("+langs[i].section(".",0,0)+") "+locale.nativeLanguageName());
        if(current==langs[i] || current.section(".",0,0)==langs[i].section(".",0,0)){
          ui->list_locale->setCurrentRow(i);
        }
      }
      ui->list_locale->scrollToItem(ui->list_locale->currentItem(), QAbstractItemView::PositionAtCenter);
    }

  }else if(checked == ui->actionInfo){
    ui->stacked_sidebar->setCurrentWidget(ui->page_system);
    if(ui->text_system_info->toPlainText().isEmpty()){
      ui->text_system_info->setHtml( BACKEND->system_information() );
      ui->text_system_rawpci->setPlainText(BACKEND->pci_info() );
      ui->toolBox_info->setCurrentWidget(ui->page_sys_info);
    }

  }else if(checked == ui->actionKeyboard){
    ui->stacked_sidebar->setCurrentWidget(ui->page_keyboard);
    //Note: The keyboard loading routine is asynchronous and this button will
    //  only become active when ready

  }else if(checked == ui->actionLog){
    ui->stacked_sidebar->setCurrentWidget(ui->page_log);

  }else{
    collapse_sidebar();
    return;
  }
  //Now show the sidebar as needed
  if(opensidebar){
    int width = ui->centralwidget->width();
    if(last_sidebar_size<=0){ last_sidebar_size = width/4; }
    ui->splitter->setSizes(QList<int>() << last_sidebar_size << width-last_sidebar_size);
    ui->splitter->handle(1)->setEnabled(true);
  }
}

void MainUI::sidebar_size_changed(){
  last_sidebar_size = ui->splitter->sizes().first();
}

void MainUI::keyboard_layout_changed(QString variant){
  //Read the list of variants for the current layout and update the variant list
  QJsonObject obj = QJsonDocument::fromJson(ui->combo_key_layout->currentData(100).toByteArray()).object();
  //qDebug() << "Got Variants:" << obj;
  ui->combo_key_variant->clear();
  ui->combo_key_variant->addItem("----");
  QStringList ids = obj.keys();
  QJsonObject sorted;
  //Sort the list
  for(int i=0; i<ids.length(); i++){
    //reverse the variable/value
    sorted.insert(obj.value(ids[i]).toString(), ids[i]);
  }
  //Now add the sorted items to the combo box
  ids = sorted.keys();
  for(int i=0; i<ids.length(); i++){
    QString index=sorted.value(ids[i]).toString();
    ui->combo_key_variant->addItem(ids[i], index);
    if(variant == index){ ui->combo_key_variant->setCurrentIndex(i+1); }
  }
  save_keyboard_layout();
}

void MainUI::save_keyboard_layout(){
  ui->keyboard_test->clear();
  QStringList current = BACKEND->keyboard(); //layout, model, variant
  QStringList now;
  now << ui->combo_key_layout->currentData().toString() << ui->combo_key_model->currentData().toString() << ui->combo_key_variant->currentData().toString();
  if( now != current ){
    qDebug() << "Set keyboard settings:" << now;
    BACKEND->setKeyboard(now, !DEBUG);
  }
}

void MainUI::populateKeyboardInfo(){
  QStringList current = BACKEND->keyboard(); //layout, model, variant
  //qDebug() << "Got Current Keyboard Settings:" << current;
  //Initial population of info
  QJsonObject json = BACKEND->availableKeyboardModels();
  QStringList tmp = json.keys();
  for(int i=0; i<tmp.length(); i++){
    ui->combo_key_model->addItem(json.value(tmp[i]).toString(), tmp[i]);
    if(tmp[i] == current[1]){ ui->combo_key_model->setCurrentIndex(i); }
  }
  json = BACKEND->availableKeyboardLayouts();
  tmp  = json.keys();
  QJsonObject sorted;
  //qDebug() << "Got Keyboard Layouts:" << json;
  // Sort the keyboard objects
  for(int i=0; i<tmp.length(); i++){
    QJsonObject tobj = json.value(tmp[i]).toObject();
    tobj.insert("id", tmp[i]);
    sorted.insert(tobj.value("description").toString(), tobj);
  }
  //Now add the sorted items to the combo box
  tmp = sorted.keys();
  for(int i=0; i<tmp.length(); i++){
    QJsonObject tobj = sorted.value(tmp[i]).toObject();
    QString id = tobj.value("id").toString();
    ui->combo_key_layout->addItem(tmp[i], id );
    if(current[0] == id){ ui->combo_key_layout->setCurrentIndex(i); }
    if(!tobj.value("variants").toObject().isEmpty()){
      ui->combo_key_layout->setItemData(i, QJsonDocument(tobj.value("variants").toObject()).toJson(), 100);
      //qDebug() << "Got Variants:" << tmp[i] << tobj;
    }
  }
  //Now update the current variant list
  keyboard_layout_changed(current[2]);
  //Now setup connections (do not change keyboard for first init)
  connect(ui->combo_key_model, SIGNAL(currentIndexChanged(int)), this, SLOT(save_keyboard_layout()) );
  connect(ui->combo_key_layout, SIGNAL(currentIndexChanged(int)), this, SLOT(keyboard_layout_changed()) );
  connect(ui->combo_key_variant, SIGNAL(currentIndexChanged(int)), this, SLOT(save_keyboard_layout()) );
  ui->actionKeyboard->setEnabled(true); //info now available
}

void MainUI::localeChanged(){
  QString now = ui->list_locale->currentItem()->text().section(")",0,0).section("(",-1);
  //qDebug() << "Change Locale:" << now;
  if((now+".UTF-8") != BACKEND->lang()){
    BACKEND->setLang(now+".UTF-8");
  }
  //Still need to change the localization object for the UI
  QString localeDir="/usr/local/share/project-trident/i18n";
  static QTranslator *curTrans = 0;
  QTranslator *newTrans = new QTranslator(this);
  QString lfile = localeDir+"/tri-install_"+now+".qm";
  if( newTrans->load("tri-install_"+now, localeDir, "", ".qm") ){
    QApplication::instance()->installTranslator(newTrans);
  }else{
    //Invalid locale file (en_US typically - default locale without a file)
    newTrans->deleteLater();
    newTrans = 0;
  }
  if(curTrans!=0){
    QApplication::instance()->removeTranslator(curTrans);
    curTrans->deleteLater();
    curTrans = 0;
  }
  curTrans = newTrans; //save for later;
  ui->retranslateUi(this);
}

void MainUI::nextClicked(){
  //Determine the next page to go to
  QWidget *cur = ui->stackedWidget->currentWidget();
  if(!savePageToBackend(cur)){ return; } //stop here if unable to continue
  QWidget *next = 0;
  int page = page_list.indexOf(cur);
  if(page>=0 && page<page_list.count()-1){ next = page_list[page+1]; }
  if(next!=0){
    loadPageFromBackend(next);
    ui->stackedWidget->setCurrentWidget(next);
  }
  updateButtonFrame();
}

void MainUI::prevClicked(){
  //Determine the previous page to go to
  QWidget *cur = ui->stackedWidget->currentWidget();
  savePageToBackend(cur, false); //preserve the current state of the page if possible (no prompts)
  QWidget *next = 0;
  int page = page_list.indexOf(cur);
  if(page>0){ next = page_list[page-1]; }

  if(next!=0){
    ui->tool_next->setEnabled(true); //just in case a page validator had it disabled for the moment
    loadPageFromBackend(next);
    ui->stackedWidget->setCurrentWidget(next);
  }
  updateButtonFrame();
}

void MainUI::startInstallClicked(){
  ui->stackedWidget->setCurrentWidget(ui->page_installing);
  updateButtonFrame();
  ui->tabWidget->setCurrentIndex(0); //start the slideshow on the first tab
  if(DEBUG){
    QTimer::singleShot(20000, this, SLOT(installFinished()));
  }else{
    BACKEND->startInstallation();
  }
  slideshowTimer->start();
}

void MainUI::newInstallMessage(QString msg){
  ui->label_installing->setText(msg.section("\n",-2,-1,QString::SectionSkipEmpty));
  bool scrolldown = (ui->text_install_log->verticalScrollBar()->value()==ui->text_install_log->verticalScrollBar()->maximum());
  ui->text_install_log->append(msg);
  if(scrolldown){ ui->text_install_log->verticalScrollBar()->setValue( ui->text_install_log->verticalScrollBar()->maximum() ); }
}

void MainUI::installFinished(bool ok){
  slideshowTimer->stop();
  if(!ok){
    QMessageBox::warning(this, tr("Installation Failed"), tr("Please view the installation log for details") );
  }
  ui->stackedWidget->setCurrentWidget(ui->page_finished);
  updateButtonFrame();
}

void MainUI::rebootClicked(){
  if(!DEBUG){ QProcess::startDetached("shutdown -r now"); }
  QApplication::exit(0);
}

void MainUI::shutdownClicked(){
  if(!DEBUG){ QProcess::startDetached("shutdown -p now"); }
  QApplication::exit(0);
}

void MainUI::updateButtonFrame(){
  QWidget *cur = ui->stackedWidget->currentWidget();
  bool showNext, showPrev, showStart, showReboot;
  showNext = showPrev = showStart = showReboot = false;
  bool showframe = true;
  if(cur == ui->page_welcome){ showNext = true; }
  else if(cur == ui->page_partitions){ showNext = showPrev = true; }
  else if(cur == ui->page_user){ showNext = showPrev = true; }
  else if(cur == ui->page_pkgs){ showNext = showPrev = true; }
  else if(cur == ui->page_summary){ showStart = showPrev = true; }
  else if(cur == ui->page_installing){ showframe = false; }
  else if(cur == ui->page_finished){ showReboot = true; }

  //Now setup the visibility of the items
  ui->frame_buttons->setVisible(showframe);
  ui->tool_next->setVisible(showNext);
  ui->tool_prev->setVisible(showPrev);
  ui->tool_startinstall->setVisible(showStart);
  ui->tool_reboot->setVisible(showReboot);
  ui->tool_shutdown->setVisible(showReboot || showNext || showStart);

  //Now setup the progress bar
  int page = page_list.indexOf(cur);
  //qDebug() << "Got Page:" << page << page_list.count() << ui->progress_pages->maximum() << ui->progress_pages->minimum();
  if(page>=0 && page<page_list.count()){
    ui->progress_pages->setValue(page+1);
  }
  ui->progress_pages->setVisible(page>=0);
  ui->actionLog->setVisible(page<0); //only show this on install/result pages
}

//Internal button/timer slots
void MainUI::nextSlideshowImage(){
  int ctab = ui->tabWidget->currentIndex();
  ctab++;
  if(ctab >= ui->tabWidget->count()){ ctab = 0; } //rollback to the start
  ui->tabWidget->setCurrentIndex(ctab);
}

//Radio button toggles
void MainUI::radio_disk_toggled(){
  // Main intall type area
  ui->list_zpools->setVisible(ui->radio_disk_bootenv->isChecked());
  ui->tree_disks->setVisible(ui->radio_disk_single->isChecked());
  // Advanced options
  ui->group_disk_encrypt->setEnabled(ui->radio_disk_single->isChecked());
  ui->group_disk_refind->setEnabled(BACKEND->isUEFI() && ui->radio_disk_single->isChecked());
  ui->group_disk_swap->setEnabled(ui->radio_disk_single->isChecked());
  validateDiskPage();
}

void MainUI::swap_size_changed(int mb){
  ui->label_disk_swap->setText( QString::number(mb)+" MB" );
  QTimer::singleShot(5, this, SLOT(validateDiskPage()) );
}

void MainUI::validateDiskPage(){
  bool ok = true;
  // Disk Space check (5GB + SWAP)
  if(ui->radio_disk_single->isChecked()){
    double minsize_mb = 5120 + (ui->group_disk_swap->isChecked() ? ui->slider_disk_swap->value() : 0);
    QTreeWidgetItem *item = ui->tree_disks->currentItem();
    if(item!=0){
      double size_mb = item->data(0, Qt::UserRole).toString().toDouble();
      ok = ok && (size_mb > minsize_mb);
    }else{
      ok = false;
    }
  }
  // Encryption key validation
  bool passok = ( ui->line_disk_pass->text().isEmpty() || (ui->line_disk_pass->text() == ui->line_disk_pass_chk->text()) );
  //qDebug() << "Got Enc Pass OK:" << passok;
  ui->tool_encrypt_passcheck->setVisible(!passok);
  if(ui->group_disk_encrypt->isChecked() && ui->group_disk_encrypt->isEnabled()){
    ok = ok && passok && !ui->line_disk_pass->text().isEmpty();
  }
  if(ui->stackedWidget->currentWidget()==ui->page_partitions){
    ui->tool_next->setEnabled(ok);
  }
}

void MainUI::userDT_changed(){
  //user changed datetime or timezone
  QDateTime dt = ui->dateTimeEdit->dateTime();
  //qDebug() << "Date Time Change:" << dt;
  dt.setTimeZone( QTimeZone( ui->combo_welcome_timezone->currentText().toUtf8() ) );
  //qDebug() << " - After changing TZ:" << dt;
  BACKEND->setDateTime(dt);
}

bool MainUI::validateRootPassword(){
  QString pass = ui->line_pass_root->text();
  bool match = (pass == ui->line_passrepeat_root->text() );
  ui->tool_root_pass_chk->setVisible(!match);
  ui->line_pass_root->setEchoMode( ui->tool_showpass_root->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
  return (!pass.isEmpty() && match);
}

bool MainUI::validateUserInfo(){
  //password match
  QString pass = ui->line_user_pass->text();
  bool match = (pass == ui->line_user_pass2->text() );
  //qDebug() << "validate User Info:" << match << pass;
  ui->tool_user_pass_chk->setVisible(!match);
  ui->line_user_pass->setEchoMode( ui->tool_user_showpass->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
  //User name/comment text validation
  ui->line_user_name->setText( ui->line_user_name->text().toLower().remove(" ") );

  bool infochk = !ui->line_user_comment->text().isEmpty() && !ui->line_user_name->text().isEmpty();
  //Return the overall result
  return (!pass.isEmpty() && match && infochk);
}

void MainUI::autogenerateUsername(){
  QString full = ui->line_user_comment->text();
  if(full.simplified().isEmpty() || !ui->line_user_name->text().isEmpty()){ return; }
  QString name = full;
  QStringList list = full.split(" ", QString::SkipEmptyParts);
  if(list.count()>1){
    //multiple words, use the first letter of each and the full last word
    name.clear();
    for(int i=0; i<list.length(); i++){
      if(i==list.length()-1){ name.append(list[i]); } //last word
      else{ name.append(list[i].at(0)); }
    }
  }
  ui->line_user_name->setText( name.toLower().remove(" ") );
}

void MainUI::validateUserPage(){
  //Note - need to combine the bools this way so that both checks are run each time
  bool ok_root = validateRootPassword();
  bool ok_user = validateUserInfo();
  ui->tool_next->setEnabled(ok_root && ok_user);
}

void MainUI::pkg_item_changed(QTreeWidgetItem *item, int col){
  //qDebug() << "Got pkg item changed";
  if(col!=0 || item->checkState(0)!=Qt::Checked){ return; }
  //qDebug() << "Got Pkg Item Changed:" << item->text(col);
  if(item->parent()!=0 ){
    if(item->parent()->data(0,100).toString().contains("single")){
      //Make sure that none of the other items are checked
      for(int i=0; i<item->parent()->childCount(); i++){
        QTreeWidgetItem *child = item->parent()->child(i);
        if(child==item){ continue; }
        else if(child->checkState(0)==Qt::Checked){ child->setCheckState(0,Qt::Unchecked); }
      }
    }
  }
}
