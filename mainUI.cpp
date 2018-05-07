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
  if(DEBUG){
    this->show();
  }else{
    this->showMaximized();
  }
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
    qDebug() << "Got Disks:" << obj;
    QStringList disks = obj.keys();
    for(int i=0; i<disks.length(); i++){
      QJsonObject info = obj.value(disks[i]).toObject();
      QTreeWidgetItem *disk = new QTreeWidgetItem(ui->tree_disks, QStringList() << disks[i]);
        disk->setIcon(0, QIcon::fromTheme(disks[i].startsWith("da") ? "media-removable" : "harddrive"));
        disk->setToolTip(0, info.value(disks[i]).toObject().value("label").toString() );
        disk->setWhatsThis(0, disks[i] + " : all"); //disk ID, partition type
      QStringList parts = info.keys();
      for(int p=0; p<parts.length(); p++){
        QJsonObject pinfo = info.value(parts[p]).toObject();
        if(disks[i] == parts[p]){
           disk->setToolTip( 0, BACKEND->diskInfoObjectToString(pinfo) );
           //Need to create the treewidgetitem for the freespace too (freemb)
           QTreeWidgetItem *part = new QTreeWidgetItem(disk, QStringList() << "Free Space ("+Backend::mbToHuman(pinfo.value("freemb").toString().toDouble())+")");
             part->setWhatsThis(0, disks[i]+" : free");
        }else{
          QTreeWidgetItem *part = new QTreeWidgetItem(disk, QStringList() << parts[p]+" ("+BACKEND->diskInfoObjectToShortString(pinfo)+")" );
            part->setWhatsThis(0, disks[i] + " : " + parts[p].remove(disks[i]) ); //disk ID, partition type
            part->setToolTip( 0, BACKEND->diskInfoObjectToString(pinfo) );
        }
      }
    }
  }
}

void MainUI::savePageToBackend(QWidget *current){
  //Note: This will never run for the installation/finished pages
  if(current == ui->page_user){
    BACKEND->clearUsers();
    userdata data;
      data.name = ui->line_user_name->text();
      data.comment = ui->line_user_comment->text();
      data.pass = ui->line_user_pass->text();
      data.shell = ui->combo_user_shell->currentText();
      data.groups << "wheel" << "operator";
      data.home = "/usr/home/"+data.name;
    BACKEND->addUser(data);
    BACKEND->setRootPass(ui->line_pass_root->text());
  }
}

//==============
//  PRIVATE SLOTS
//==============
void MainUI::nextClicked(){
  //Determine the next page to go to
  QWidget *cur = ui->stackedWidget->currentWidget();
  savePageToBackend(cur);
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
  QWidget *next = 0;
  int page = page_list.indexOf(cur);
  if(page>0){ next = page_list[page-1]; }

  if(next!=0){
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

void MainUI::validateRootPassword(){

}

void MainUI::validateUserInfo(){

}
