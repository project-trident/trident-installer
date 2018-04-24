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
  setupConnections();
  page_list << ui->page_welcome << ui->page_partitions << ui->page_user << ui->page_pkgs << ui->page_summary;
  //NOTE: page_installing and page_finished are always at the end of the list;
  ui->progress_pages->setRange(1,page_list.count()+1);
  ui->stackedWidget->setCurrentWidget(page_list.first());
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
}

// ===============
//    PUBLIC FUNCTIONS
// ===============


//==============
//  PRIVATE
//==============


//==============
//  PRIVATE SLOTS
//==============
void MainUI::nextClicked(){
 //Determine the next page to go to
  QWidget *cur = ui->stackedWidget->currentWidget();
  QWidget *next = 0;
  int page = page_list.indexOf(cur);
  if(page>=0 && page<page_list.count()-1){ next = page_list[page+1]; }

  if(next!=0){ ui->stackedWidget->setCurrentWidget(next); }
  updateButtonFrame();
}

void MainUI::prevClicked(){
  //Determine the previous page to go to
  QWidget *cur = ui->stackedWidget->currentWidget();
  QWidget *next = 0;
  int page = page_list.indexOf(cur);
  if(page>0){ next = page_list[page-1]; }

  if(next!=0){ ui->stackedWidget->setCurrentWidget(next); }
  updateButtonFrame();
}

void MainUI::startInstallClicked(){
  ui->stackedWidget->setCurrentWidget(ui->page_installing);
  updateButtonFrame();
  if(DEBUG){
    QTimer::singleShot(5000, this, SLOT(installFinished()));
  }else{
    //Start Backend Process (TODO)
  }
}

void MainUI::installFinished(){
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
  qDebug() << "Got Page:" << page << page_list.count() << ui->progress_pages->maximum() << ui->progress_pages->minimum();
  if(page>=0 && page<page_list.count()){
    ui->progress_pages->setValue(page+1);
  }
  ui->progress_pages->setVisible(page>=0);

}
