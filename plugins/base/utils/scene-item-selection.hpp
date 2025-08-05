#pragma once
#include "scene-selection.hpp"
#include "filter-combo-box.hpp"
#include "variable-spinbox.hpp"
#include "variable-line-edit.hpp"
#include "variable-string.hpp"
#include "regex-config.hpp"

#include <obs-data.h>
#include <QLayout>

namespace advss {

class SceneItemSelection {
public:
	void Save(obs_data_t *obj,
		  const char *name = "sceneItemSelection") const;
	void Load(obs_data_t *obj, const char *name = "sceneItemSelection");
	// TODO: Remove in future version
	void Load(obs_data_t *obj, const char *name, const char *targetName,
		  const char *idxName);

	enum class Type {
		SOURCE_NAME,
		VARIABLE_NAME,
		SOURCE_NAME_PATTERN = 10,
		SOURCE_GROUP = 15,
		SOURCE_TYPE = 20,
		INDEX = 30,
		INDEX_RANGE = 40,
		ALL = 50,
		ANY = 60,
	};

	// Name conflicts can happen if multiple instances of a given source are
	// present in a given scene.
	//
	// If that is the case, the user has the option to specify if all / any
	// or a given individual instance of the source based on its index shall
	// be returned.
	enum class NameConflictSelection {
		ALL,
		ANY,
		INDIVIDUAL,
	};

	void SetType(Type t) { _type = t; }
	Type GetType() const { return _type; }
	NameConflictSelection GetIndexType() const;
	std::vector<OBSSceneItem> GetSceneItems(const SceneSelection &) const;
	bool IsSelectionOfTypeAny() const;
	std::string ToString(bool resolve = false) const;

	// TODO: Remove in future version
	//
	// Only exists to enable backwards compatibility with older versions of
	// scene item visibility action
	void SetSourceTypeSelection(const char *);

	void ResolveVariables();

private:
	std::vector<OBSSceneItem>
	GetSceneItemsByName(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetSceneItemsByPattern(const SceneSelection &) const;
	std::vector<OBSSceneItem> GetSceneItemsOfGroup() const;
	std::vector<OBSSceneItem>
	GetSceneItemsByType(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetSceneItemsByIdx(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetAllSceneItems(const SceneSelection &) const;

	void ReduceBasedOnIndexSelection(std::vector<OBSSceneItem> &) const;

	Type _type = Type::SOURCE_NAME;

	OBSWeakSource _source;
	std::weak_ptr<Variable> _variable;
	IntVariable _index = 1;
	IntVariable _indexEnd = 1;
	NameConflictSelection _nameConflictSelectionType =
		NameConflictSelection::ALL;
	int _nameConflictSelectionIndex = 0;
	std::string _sourceType;
	StringVariable _pattern = ".*";
	RegexConfig _regex = RegexConfig(true);
	friend class SceneItemSelectionWidget;
};

class SceneItemSelectionWidget : public QWidget {
	Q_OBJECT

public:
	enum class NameClashMode { NONE, ALL, ANY, ANY_AND_ALL, HIDE };
	explicit SceneItemSelectionWidget(
		QWidget *parent,
		const std::vector<SceneItemSelection::Type> &selections =
			{
				SceneItemSelection::Type::SOURCE_NAME,
				SceneItemSelection::Type::VARIABLE_NAME,
				SceneItemSelection::Type::SOURCE_NAME_PATTERN,
				SceneItemSelection::Type::SOURCE_GROUP,
				SceneItemSelection::Type::SOURCE_TYPE,
				SceneItemSelection::Type::INDEX,
				SceneItemSelection::Type::INDEX_RANGE,
				SceneItemSelection::Type::ALL,
				SceneItemSelection::Type::ANY,
			},
		NameClashMode mode = NameClashMode::ANY_AND_ALL);
	void SetSceneItem(const SceneItemSelection &);
	void SetScene(const SceneSelection &);

protected:
	void showEvent(QShowEvent *event) override;

signals:
	void SceneItemChanged(const SceneItemSelection &);

private slots:
	// Name based
	void SceneChanged(const SceneSelection &);
	void VariableChanged(const QString &);
	void SourceChanged(int);
	void SourceGroupChanged(int);
	void NameConflictIndexChanged(int);
	void PatternChanged();
	void RegexChanged(const RegexConfig &);

	// Source type based
	void SourceTypeChanged(const QString &);

	// Index based
	void IndexChanged(const NumberVariable<int> &);
	void IndexEndChanged(const NumberVariable<int> &);

	void ChangeType();

private:
	void ClearWidgets();
	void PopulateItemSelection();
	void SetupNameConflictIdxSelection(int);
	void SetNameConflictVisibility();
	void SetWidgetVisibility();

	QHBoxLayout *_controlsLayout;
	FilterComboBox *_sources;
	FilterComboBox *_sourceGroups;
	VariableSelection *_variables;
	QComboBox *_nameConflictIndex;
	VariableSpinBox *_index;
	VariableSpinBox *_indexEnd;
	QComboBox *_sourceTypes;
	VariableLineEdit *_pattern;
	RegexConfigWidget *_regex;
	QPushButton *_changeType;

	SceneSelection _scene;
	SceneItemSelection _currentSelection;
	NameClashMode _nameClashMode = NameClashMode::ALL;
	const std::vector<SceneItemSelection::Type> _selectionTypes;
};

class SceneItemTypeSelection : public QDialog {
	Q_OBJECT

public:
	SceneItemTypeSelection(QWidget *parent,
			       const SceneItemSelection::Type &type);
	static bool AskForSettings(
		QWidget *parent, SceneItemSelection::Type &type,
		const std::vector<SceneItemSelection::Type> &allowedTypes);

private:
	QComboBox *_typeSelection;
	QDialogButtonBox *_buttonbox;
};

} // namespace advss
