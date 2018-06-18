//===========================================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//===========================================
#include "mainUI.h"
#include "ui_mainUI.h"
#include <QDebug>

#define DEBUG 1

MainUI::MainUI() : QMainWindow(), ui(new Ui::MainUI){
  ui->setupUi(this); //load the designer file
  BACKEND = new Backend(this);
  //PAGE ORDER
  page_list << ui->page_welcome << ui->page_partitions << ui->page_user << ui->page_pkgs << ui->page_summary;
  //NOTE: page_installing and page_finished are always at the end of the list;

  //Now setup any other UI elements
  ui->progress_pages->setRange(0,page_list.count());
  ui->stackedWidget->setCurrentWidget(page_list.first());
  loadPageFromBackend(page_list.first());
  //Create internal timers/objects
  slideshowTimer = new QTimer(this);
    slideshowTimer->setInterval(3000);

  //Update the visuals and show the window
  setupConnections();
  updateButtonFrame();
  //Only load the package page once
  BACKEND->populatePackageTreeWidget(ui->tree_pkgs);
  if(DEBUG){
    this->show();
  }else{
    this->showMaximized();
  }
  //BACKEND->availableKeyboardModels();
}

MainUI::~MainUI(){

}

void MainUI::setupConnections(){
  connect(ui->tool_next, SIGNAL(clicked()), this, SLOT(nextClicked()) );
  connect(ui->tool_prev, SIGNAL(clicked()), this, SLOT(prevClicked()) );
  connect(ui->tool_startinstall, SIGNAL(clicked()), this, SLOT(startInstallClicked()) );
  connect(ui->tool_reboot, SIGNAL(clicked()), this, SLOT(rebootClicked()) );
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

  }else if(current == ui->page_user){
    if(ui->combo_user_shell->count()<1){
      //Need to fill the list of available shells
      QStringList shells = BACKEND->availableShells();
      QString defaultshell = BACKEND->defaultUserShell();
      for(int i=0; i<shells.length(); i++){
        ui->combo_user_shell->addItem( shells[i].section("/",-1), shells[i]);
        if(shells[i] == defaultshell){ ui->combo_user_shell->setCurrentIndex(i); }
      }
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
    if(disk.install_partition=="all"){
      double swapsize = 1024;
      if(totalMB<(15*1024)){ swapsize=512; } //small disk - decrease the swap size accordingly
      else if(totalMB>(200*1024)){ swapsize=2048; } //large disk - increase swap a bit more
      //create a SWAP partition on full-disk installs
      partitiondata swap;
        swap.install_type = "SWAP.eli"; //make sure we use encrypted swap by default
        swap.sizeMB = swapsize; //1GB swap by default
      disk.partitions << swap;
      totalMB-=swapsize; //decrease the total size a bit to account for the swap partition
    }
    //Now setup the main partition for ZFS
    partitiondata part;
      part.install_type = "ZFS";
      part.sizeMB = totalMB - 10; //Make it 10MB smaller than calculated to account for rounding errors and such
      part.create_partitions  = BACKEND->generateDefaultZFSPartitions();
    disk.partitions.prepend(part); //make sure this partition is always first
    //Now save it into the backend
    BACKEND->addDisk(disk);
  }
  return true;
}

//==============
//  PRIVATE SLOTS
//==============
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
  if(DEBUG){
    QTimer::singleShot(10000, this, SLOT(installFinished()));
  }else{
    //Start Backend Process (TODO)
  }
  slideshowTimer->start();
}

void MainUI::installFinished(){
  slideshowTimer->stop();
  if(!DEBUG){
    //Do Error checking here (TODO)
  }
  ui->stackedWidget->setCurrentWidget(ui->page_finished);
  updateButtonFrame();
}

void MainUI::rebootClicked(){
  if(!DEBUG){ QProcess::startDetached("shutdown -r now"); }
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
  //Now setup the progress bar
  int page = page_list.indexOf(cur);
  //qDebug() << "Got Page:" << page << page_list.count() << ui->progress_pages->maximum() << ui->progress_pages->minimum();
  if(page>=0 && page<page_list.count()){
    ui->progress_pages->setValue(page+1);
  }
  ui->progress_pages->setVisible(page>=0);
}

//Internal button/timer slots
void MainUI::nextSlideshowImage(){
  int ctab = ui->tabWidget->currentIndex();
  ctab++;
  if(ctab >= ui->tabWidget->count()){ ctab = 0; } //rollback to the start
  ui->tabWidget->setCurrentIndex(ctab);
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
