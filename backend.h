//============================
//  Project Trident source code
//  Copyright (c) 2018, Ken Moore
//  Available under the 2-clause BSD license
//  See the LICENSE file for full details
//============================
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QJsonArray>
#include <QTimeZone>
#include <QDebug>

struct userdata{
	QString name, comment, pass, shell, home;
	QStringList groups;
	bool autologin;
};

class Backend : public QObject{
	Q_OBJECT
private:
	QJsonObject settings;

	QString generateInstallConfig(); //turns JSON settings into a config file for pc-sysinstall

private slots:

public:
	Backend(QObject *parent);
	~Backend();

	//Localization
	QString lang();
	void setLang(QString);
	QStringList availableLanguages();
	//Keyboard Settings
	QStringList keyboard(); //layout, model, variant
	void setKeyboard(QStringList); //layout, model, variant
	QStringList availableKeyboards(QString layout = "", QString model = "");
	//Time Settings
	QString timezone();
	void setTimezone(QString);
	QDateTime localDateTime();
	void setDateTime(QDateTime); //alternate to setTimezone() - sets both timezone and offset from system time
	QStringList availableTimezones();
	bool useNTP();
	void setUseNTP(bool);

	//System Users
	QString rootPass();
	void setRootPass(QString);
	QList<userdata> users;
	void addUser(userdata); //will overwrite existing user with the same "name"
	void removeUser(QString name);
	void clearUsers(); //remove all users

	//Disk Partitioning

	//Packages

public slots:

signals:

};
