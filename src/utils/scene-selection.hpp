#pragma once
#include "scene-group.hpp"
#include "variable.hpp"
#include "utility.hpp"

#include <QComboBox>

class SceneSelection {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj, const char *name = "scene",
		  const char *typeName = "sceneType");

	enum class Type {
		SCENE,
		GROUP,
		PREVIOUS,
		CURRENT,
		PREVIEW,
		VARIABLE,
	};

	Type GetType() const { return _type; }
	OBSWeakSource GetScene(bool advance = true) const;
	std::string ToString() const;

private:
	OBSWeakSource _scene;
	SceneGroup *_group = nullptr;
	std::weak_ptr<Variable> _variable;
	Type _type = Type::SCENE;
	friend class SceneSelectionWidget;
};

class SceneSelectionWidget : public QComboBox {
	Q_OBJECT

public:
	SceneSelectionWidget(QWidget *parent, bool variables = false,
			     bool sceneGroups = false, bool previous = false,
			     bool current = false, bool preview = false);
	void SetScene(const SceneSelection &);
signals:
	void SceneChanged(const SceneSelection &);

private slots:
	void SelectionChanged(const QString &name);
	void ItemAdd(const QString &name);
	void ItemRemove(const QString &name);
	void ItemRename(const QString &oldName, const QString &newName);

private:
	void Reset();
	SceneSelection CurrentSelection();
	void PopulateSelection();
	bool IsCurrentSceneSelected(const QString &name);
	bool IsPreviousSceneSelected(const QString &name);
	bool IsPreviewSceneSelected(const QString &name);
	bool NameUsed(const QString &name);

	bool _current;
	bool _previous;
	bool _preview;
	bool _variables;
	bool _sceneGroups;

	SceneSelection _currentSelection;

	// Order of entries
	// 1. "select entry" entry
	// 2. Current / previous / preview scene
	// 3. Variables
	// 4. Scene groups
	// 5. Regular scenes
	const int _selectIdx = 0;
	int _placeholderEndIdx = -1;
	int _variablesEndIdx = -1;
	int _groupsEndIdx = -1;
	int _scenesEndIdx = -1;
};
