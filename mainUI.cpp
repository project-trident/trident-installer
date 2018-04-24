//===========================================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//===========================================
#include "mainUI.h"
#include "ui_mainUI.h"

MainUI::MainUI() : QMainWindow(), ui(new Ui::MainUI){
  ui->setupUi(this); //load the designer file
  setupConnections();
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

}

void MainUI::prevClicked(){

}

void MainUI::startInstallClicked(){

}

void MainUI::rebootClicked(){

}

void MainUI::updateButtonFrame(){

}
