//===========================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//===========================
#ifndef _TRIDENT_INSTALL_WINDOW_H
#define _TRIDENT_INSTALL_WINDOW_H

#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <QShortcut>
#include <QMessageBox>

#include "backend.h"


namespace Ui{
	class MainUI;
};

class MainUI : public QMainWindow{
	Q_OBJECT
public:
	MainUI();
	~MainUI();

	void setupConnections();

public slots:

private:
	Ui::MainUI *ui;
	QList<QWidget*> page_list;
	QTimer* slideshowTimer;
	Backend *BACKEND;

	void loadPageFromBackend(QWidget *current);
	bool savePageToBackend(QWidget *current, bool prompts = true);

private slots:
	void nextClicked();
	void prevClicked();
	void startInstallClicked();
	void installFinished();
	void rebootClicked();
	void updateButtonFrame();

	//Internal button/timer slots
	void nextSlideshowImage();
	void userDT_changed(); //user changed datetime or timezone
	bool validateRootPassword();
	bool validateUserInfo();
	void autogenerateUsername();
	void validateUserPage();

signals:

};

#endif
