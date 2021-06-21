#pragma once
#include "scene-group.hpp"
#include "utility.hpp"

#include <QComboBox>

enum class SceneSelectionType {
	SCENE,
	GROUP,
	PREVIOUS,
	CURRENT,
};

class SceneSelection {
public:
	void Save(obs_data_t *obj, const char *name = "scene",
		  const char *typeName = "sceneType");
	void Load(obs_data_t *obj, const char *name = "scene",
		  const char *typeName = "sceneType");

	SceneSelectionType GetType() { return _type; }
	OBSWeakSource GetScene(bool advance = true);
	std::string ToString();

private:
	OBSWeakSource _scene;
	SceneGroup *_group = nullptr;
	SceneSelectionType _type = SceneSelectionType::SCENE;
	friend class SceneSelectionWidget;
};

class SceneSelectionWidget : public QComboBox {
	Q_OBJECT

public:
	SceneSelectionWidget(QWidget *parent, bool sceneGroups = false,
			     bool previous = false, bool current = false);
	void SetScene(SceneSelection &);
signals:
	void SceneChanged(const SceneSelection &);

private slots:
	void SelectionChanged(const QString &name);
	void SceneGroupAdd(const QString &name);
	void SceneGroupRemove(const QString &name);
	void SceneGroupRename(const QString &oldName, const QString &newName);

private:
	bool IsCurrentSceneSelected(const QString &name);
	bool IsPreviousSceneSelected(const QString &name);
};
