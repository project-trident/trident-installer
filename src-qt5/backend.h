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
#include <QtConcurrent>
#include <QTreeWidget>
#include <QTreeWidgetItem>

struct userdata{
	QString name, comment, pass, shell, home;
	QStringList groups;
	bool autologin;
};

struct partitiondata{
	QString install_type; //install_type: "ZFS", "SWAP", "UFS"
	QStringList create_partitions; //datasets if using ZFS, partitions if using something else
	double sizeMB; //size in MB; set to <=0 for automatic
	QString extrasetup;
};

struct diskdata{
	QString name, install_partition; //name: ada0, install_partition: "all", "free", "s1", "s2" ...
	QString mirror_disk; //Used for old-style gmirror setup
	bool installBootManager;
	QList<partitiondata> partitions;
};



class Backend : public QObject{
	Q_OBJECT
private:
	QJsonObject settings, keyboardInfo;
	QList<userdata> USERS;
	QList<diskdata> DISKS;

	QString generateInstallConfig(); //turns JSON settings into a config file for pc-sysinstall

	void checkKeyboardInfo();

	void GeneratePackageItem(QJsonObject json, QTreeWidget *tree, QString name, QTreeWidgetItem *parent = 0);

private slots:

public:
	Backend(QObject *parent);
	~Backend();

	static QString runCommand(bool &success, QString command, QStringList arguments = QStringList(), QString workdir = "", QStringList env = QStringList());
	static QString mbToHuman(double);
	static QString readFile(QString path);

	// Information
	QString generateSummary(){ return generateInstallConfig(); }
	bool isLaptop();


	//Localization
	QString lang();
	void setLang(QString);
	QStringList availableLanguages();
	//Keyboard Settings
	QStringList keyboard(); //layout, model, variant
	void setKeyboard(QStringList); //layout, model, variant
	QJsonObject availableKeyboardModels();
	QJsonObject availableKeyboardLayouts();
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
	QList<userdata> users(){ return USERS; }
	void addUser(userdata); //will overwrite existing user with the same "name"
	void removeUser(QString name);
	void clearUsers(); //remove all users

	//Disk Partitioning
	// - Information functions
	QJsonObject availableDisks();
	QString diskInfoObjectToString(QJsonObject obj);
	QString diskInfoObjectToShortString(QJsonObject obj);
	bool checkValidSize(QJsonObject obj, bool installdrive = true, bool freespaceonly = false);
	// ZFS Install to BE option
	bool installToBE(); //will report true if a valid ZFS pool was designated
	QString zpoolName(); //will return the designated ZFS pool name
	void setInstallToBE(QString pool); //set to an empty string to disable installing to a BE
	//Individual disk setup
	QList<diskdata> disks(){ return DISKS; }
	void addDisk(diskdata); //will overwrite existing disk with the same name
	void removeDisk(QString name);
	void clearDisks();
	QStringList generateDefaultZFSPartitions();

	//Packages
	QStringList availableShells();
	QString defaultUserShell();
	void populatePackageTreeWidget(QTreeWidget *tree);


public slots:

signals:

};
