#pragma once
#include "filter-combo-box.hpp"

#include <obs.hpp>

namespace advss {

struct SceneGroup;
class Variable;

class SceneSelection {
public:
	EXPORT void Save(obs_data_t *obj) const;
	EXPORT void Load(obs_data_t *obj, const char *name = "scene",
			 const char *typeName = "sceneType");

	enum class Type {
		SCENE,
		GROUP,
		PREVIOUS,
		CURRENT,
		PREVIEW,
		VARIABLE,
	};

	EXPORT Type GetType() const { return _type; }
	EXPORT OBSWeakSource GetScene(bool advance = true) const;
	EXPORT std::string ToString(bool resolve = false) const;

private:
	OBSWeakSource _scene;
	SceneGroup *_group = nullptr;
	std::weak_ptr<Variable> _variable;
	Type _type = Type::SCENE;
	friend class SceneSelectionWidget;
};

class SceneSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	EXPORT SceneSelectionWidget(QWidget *parent, bool variables = false,
				    bool sceneGroups = false,
				    bool previous = false, bool current = false,
				    bool preview = false);
	EXPORT void SetScene(const SceneSelection &);
signals:
	void SceneChanged(const SceneSelection &);

private slots:
	EXPORT void SelectionChanged(int);
	EXPORT void ItemAdd(const QString &name);
	EXPORT void ItemRemove(const QString &name);
	EXPORT void ItemRename(const QString &oldName, const QString &newName);

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

} // namespace advss
