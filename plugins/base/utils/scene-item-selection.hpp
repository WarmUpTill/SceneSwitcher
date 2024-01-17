#pragma once
#include "scene-selection.hpp"
#include "filter-combo-box.hpp"
#include "variable-spinbox.hpp"
#include "variable-line-edit.hpp"
#include "variable-string.hpp"
#include "regex-config.hpp"
#include "utility.hpp"

#include <obs-data.h>

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
		SOURCE_GROUP = 20,
		INDEX = 30,
		INDEX_RANGE = 40,
		ALL = 50,
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

	Type GetType() const { return _type; }
	NameConflictSelection GetIndexType() const;
	std::vector<OBSSceneItem> GetSceneItems(const SceneSelection &) const;
	std::string ToString(bool resolve = false) const;

	// TODO: Remove in future version
	//
	// Only exists to enable backwards compatabilty with older versions of
	// scene item visibility action
	void SetSourceTypeSelection(const char *);

private:
	std::vector<OBSSceneItem>
	GetSceneItemsByName(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetSceneItemsByPattern(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetSceneItemsByGroup(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetSceneItemsByIdx(const SceneSelection &) const;
	std::vector<OBSSceneItem>
	GetAllSceneItems(const SceneSelection &) const;

	void ReduceBadedOnIndexSelection(std::vector<OBSSceneItem> &) const;

	Type _type = Type::SOURCE_NAME;

	OBSWeakSource _source;
	std::weak_ptr<Variable> _variable;
	IntVariable _index = 1;
	IntVariable _indexEnd = 1;
	NameConflictSelection _nameConflictSelectionType =
		NameConflictSelection::ALL;
	int _nameConflictSelectionIndex = 0;
	std::string _sourceGroup;
	StringVariable _pattern = ".*";
	RegexConfig _regex = RegexConfig(true);
	friend class SceneItemSelectionWidget;
};

class SceneItemSelectionWidget : public QWidget {
	Q_OBJECT

public:
	enum class Placeholder { ALL, ANY };
	SceneItemSelectionWidget(QWidget *parent, bool addPlaceholder = true,
				 Placeholder placeholderType = Placeholder::ALL);
	void SetSceneItem(const SceneItemSelection &);
	void SetScene(const SceneSelection &);
	void ShowPlaceholder(bool);
	void SetPlaceholderType(Placeholder t, bool resetSelection = true);
signals:
	void SceneItemChanged(const SceneItemSelection &);

private slots:
	// Name based
	void SceneChanged(const SceneSelection &);
	void VariableChanged(const QString &);
	void SourceChanged(int);
	void NameConflictIndexChanged(int);
	void PatternChanged();
	void RegexChanged(const RegexConfig &);

	// Source group based
	void SourceGroupChanged(const QString &);

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
	VariableSelection *_variables;
	QComboBox *_nameConflictIndex;
	VariableSpinBox *_index;
	VariableSpinBox *_indexEnd;
	QComboBox *_sourceGroups;
	VariableLineEdit *_pattern;
	RegexConfigWidget *_regex;
	QPushButton *_changeType;

	SceneSelection _scene;
	SceneItemSelection _currentSelection;
	bool _hasPlaceholderEntry = false;
	Placeholder _placeholder = Placeholder::ALL;

	bool _showTypeSelection = false;
};

class SceneItemTypeSelection : public QDialog {
	Q_OBJECT

public:
	SceneItemTypeSelection(QWidget *parent,
			       const SceneItemSelection::Type &type);
	static bool AskForSettings(QWidget *parent,
				   SceneItemSelection::Type &type);

private:
	QComboBox *_typeSelection;
	QDialogButtonBox *_buttonbox;
};

} // namespace advss
