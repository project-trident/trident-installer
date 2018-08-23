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
#include <QActionGroup>
#include <QAction>

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
	QActionGroup *sidebar_group;
	int last_sidebar_size;
	Backend *BACKEND;
	bool DEBUG;

	void loadPageFromBackend(QWidget *current);
	bool savePageToBackend(QWidget *current, bool prompts = true);

private slots:
	// Sidebar slots
	void collapse_sidebar();
	void sidebar_item_changed();
	void sidebar_size_changed();
	void keyboard_layout_changed(QString variant = "");
	void save_keyboard_layout();
	void populateKeyboardInfo();
	void localeChanged();

	// Button frame slots
	void nextClicked();
	void prevClicked();
	void startInstallClicked();
	void newInstallMessage(QString);
	void installFinished(bool ok = true);
	void rebootClicked();
	void shutdownClicked();
	void closeGuiClicked();
	void updateButtonFrame();

	//Radio button toggles
	void radio_disk_toggled();
	void swap_size_changed(int);
	void validateDiskPage();

	//Internal button/timer slots
	void nextSlideshowImage();
	void userDT_changed(); //user changed datetime or timezone
	bool validateRootPassword();
	bool validateUserInfo();
	void autogenerateUsername();
	void validateUserPage();
	void pkg_item_changed(QTreeWidgetItem *item, int col);

signals:

};

#endif
