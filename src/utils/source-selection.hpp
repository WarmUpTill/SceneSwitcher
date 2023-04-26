#pragma once

#include "variable.hpp"
#include "utility.hpp"

#include <QComboBox>

namespace advss {

class SourceSelection {
public:
	void Save(obs_data_t *obj, const char *name = "source") const;
	void Load(obs_data_t *obj, const char *name = "source");

	enum class Type {
		SOURCE,
		VARIABLE,
	};

	Type GetType() const { return _type; }
	OBSWeakSource GetSource() const;
	void SetSource(OBSWeakSource);
	std::string ToString(bool resolve = false) const;

	bool operator==(const SourceSelection &) const;

private:
	// TODO: Remove in future version
	// Used for backwards compatability to older settings versions
	void LoadFallback(obs_data_t *obj, const char *name);

	OBSWeakSource _source;
	std::weak_ptr<Variable> _variable;
	Type _type = Type::SOURCE;
	friend class SourceSelectionWidget;
};

class SourceSelectionWidget : public QComboBox {
	Q_OBJECT

public:
	SourceSelectionWidget(QWidget *parent, const QStringList &sourceNames,
			      bool addVariables = true);
	void SetSource(const SourceSelection &);
	void SetSourceNameList(const QStringList &);
signals:
	void SourceChanged(const SourceSelection &);

private slots:
	void SelectionChanged(const QString &name);
	void ItemAdd(const QString &name);
	void ItemRemove(const QString &name);
	void ItemRename(const QString &oldName, const QString &newName);

private:
	void Reset();
	SourceSelection CurrentSelection();
	void PopulateSelection();
	bool NameUsed(const QString &name);

	bool _addVariables;
	QStringList _sourceNames;
	SourceSelection _currentSelection;

	// Order of entries
	// 1. "select entry" entry
	// 2. Variables
	// 3. Regular sources
	const int _selectIdx = 0;
	int _variablesEndIdx = -1;
	int _sourcesEndIdx = -1;
};

} // namespace advss
