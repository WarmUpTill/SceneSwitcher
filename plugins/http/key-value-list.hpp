#pragma once
#include "string-list.hpp"
#include "variable-line-edit.hpp"

namespace advss {

class KeyValueListEdit final : public ListEditor {
	Q_OBJECT

public:
	KeyValueListEdit(QWidget *parent, const QString &addKeyString,
			 const QString &addKeyStringDescription,
			 const QString &addValueString,
			 const QString &addValueStringDescription);
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
	void MoveStringListIdxUp(int);
	bool AskForKeyValue(StringVariable &key, StringVariable &value);
	void AppendListEntryWidget(const StringVariable &key,
				   const StringVariable &value);

	StringList _stringList;
	QString _addKeyString;
	QString _addKeyStringDescription;
	QString _addValueString;
	QString _addValueStringDescription;
};

class KeyValueListContainerWidget final : public QWidget {
	Q_OBJECT

public:
	KeyValueListContainerWidget(QWidget *parent, int index);

private:
	QLabel *_key;
	QLabel *_value;

	int _index = -1;
	friend class KeyValueListEdit;
};

} // namespace advss
