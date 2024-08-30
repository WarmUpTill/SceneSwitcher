#pragma once
#include "export-symbol-helper.hpp"
#include "list-editor.hpp"
#include "obs-module-helper.hpp"
#include "variable-string.hpp"

#include <obs-data.h>

namespace advss {

class StringList : public QList<StringVariable> {
public:
	EXPORT bool Save(obs_data_t *obj, const char *name,
			 const char *elementName = "string") const;
	EXPORT bool Load(obs_data_t *obj, const char *name,
			 const char *elementName = "string");
	EXPORT void ResolveVariables();

	friend class StringListEdit;
};

class ADVSS_EXPORT StringListEdit final : public ListEditor {
	Q_OBJECT

public:
	StringListEdit(QWidget *parent, const QString &addString = "",
		       const QString &addStringDescription = "",
		       int maxStringSize = 170, bool allowEmtpy = false);
	void SetStringList(const StringList &);
	void SetMaxStringSize(int);

private slots:
	void Add();
	void Remove();
	void Up();
	void Down();
	void Clicked(QListWidgetItem *);
signals:
	void StringListChanged(const StringList &);

private:
	StringList _stringList;

	QString _addString;
	QString _addStringDescription;
	int _maxStringSize = 170;
	bool _allowEmpty = false;
};

} // namespace advss
