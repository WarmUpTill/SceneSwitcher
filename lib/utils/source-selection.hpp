#pragma once
#include "filter-combo-box.hpp"
#include "obs.hpp"

namespace advss {

class Variable;

class SourceSelection {
public:
	EXPORT void Save(obs_data_t *obj, const char *name = "source") const;
	EXPORT void Load(obs_data_t *obj, const char *name = "source");

	enum class Type {
		SOURCE,
		VARIABLE,
	};

	EXPORT Type GetType() const { return _type; }
	EXPORT OBSWeakSource GetSource() const;
	EXPORT void SetSource(OBSWeakSource);
	EXPORT void ResolveVariables();
	EXPORT std::string ToString(bool resolve = false) const;

	EXPORT bool operator==(const SourceSelection &) const;

private:
	// TODO: Remove in future version
	// Used for backwards compatibility to older settings versions
	void LoadFallback(obs_data_t *obj, const char *name);

	OBSWeakSource _source;
	std::weak_ptr<Variable> _variable;
	Type _type = Type::SOURCE;
	friend class SourceSelectionWidget;
};

class ADVSS_EXPORT SourceSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	SourceSelectionWidget(QWidget *parent, const QStringList &sourceNames,
			      bool addVariables = true);
	void SetSource(const SourceSelection &);
	void SetSourceNameList(const QStringList &);
signals:
	void SourceChanged(const SourceSelection &);

private slots:
	void SelectionChanged(int);
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
