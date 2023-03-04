#pragma once
#include <obs-data.h>
#include <obs-module.h>

#include <QPushButton>
#include <QListWidget>
#include <QStringList>

class StringList : public QStringList {
public:
	bool Save(obs_data_t *obj, const char *name,
		  const char *elementName = "string") const;
	bool Load(obs_data_t *obj, const char *name,
		  const char *elementName = "string");

	friend class StringListEdit;
};

class StringListEdit : public QWidget {
	Q_OBJECT

public:
	StringListEdit(QWidget *parent, const QString &addString = "",
		       const QString &addStringDescription = "");
	void SetStringList(const StringList &);

private slots:
	void Add();
	void Remove();
	void Up();
	void Down();
	void Clicked(QListWidgetItem *);
signals:
	void StringListChanged(const StringList &);

private:
	void SetListSize();

	StringList _stringList;

	QListWidget *_list;
	QPushButton *_add;
	QPushButton *_remove;
	QPushButton *_up;
	QPushButton *_down;

	QString _addString;
	QString _addStringDescription;
};
