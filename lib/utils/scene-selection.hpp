#pragma once
#include "canvas-helpers.hpp"

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
	EXPORT void SetScene(const OBSWeakSource &scene);
	EXPORT OBSWeakSource GetScene(bool advance = true) const;
	EXPORT OBSWeakCanvas GetCanvas() const { return _canvas; };
	EXPORT std::string ToString(bool resolve = false) const;
	EXPORT void ResolveVariables();

private:
	OBSWeakSource _scene;
	OBSWeakCanvas _canvas;
	SceneGroup *_group = nullptr;
	std::weak_ptr<Variable> _variable;
	Type _type = Type::SCENE;
	friend class SceneSelectionWidget;
};

class SceneSelectionWidget : public QWidget {
	Q_OBJECT

public:
	EXPORT SceneSelectionWidget(QWidget *parent, bool variables = false,
				    bool sceneGroups = false,
				    bool previous = false, bool current = false,
				    bool preview = false);
	EXPORT void SetScene(const SceneSelection &);
	EXPORT void LockToMainCanvas();

protected:
	void showEvent(QShowEvent *event) override;

signals:
	void SceneChanged(const SceneSelection &);
	void CanvasChanged(const OBSWeakCanvas &);

private slots:
	EXPORT void SelectionChanged(int);
	EXPORT void CanvasChangedSlot(const OBSWeakCanvas &);
	EXPORT void ItemAdd(const QString &name);
	EXPORT void ItemRemove(const QString &name);
	EXPORT void ItemRename(const QString &oldName, const QString &newName);

private:
	void Reset();
	SceneSelection CurrentSelection();
	void PopulateSceneSelection(obs_weak_canvas_t *canvas);
	bool IsCurrentSceneSelected(const QString &name);
	bool IsPreviousSceneSelected(const QString &name);
	bool IsPreviewSceneSelected(const QString &name);
	bool NameUsed(const QString &name);
	void Resize();

	FilterComboBox *_scenes;
	CanvasSelection *_canvas;

	bool _current;
	bool _previous;
	bool _preview;
	bool _variables;
	bool _sceneGroups;

	bool _forceMainCanvas = false;

	SceneSelection _currentSelection;
	bool _isPopulated = false;

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
