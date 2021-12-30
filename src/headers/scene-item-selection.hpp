#pragma once
#include <QComboBox>
#include <obs-data.h>

#include "scene-selection.hpp"
#include "utility.hpp"

class SceneItemSelection {
public:
	void Save(obs_data_t *obj, const char *name = "sceneItem",
		  const char *targetName = "sceneItemTarget",
		  const char *idxName = "sceneItemIdx");
	void Load(obs_data_t *obj, const char *name = "sceneItem",
		  const char *targetName = "sceneItemTarget",
		  const char *idxName = "sceneItemIdx");

	enum class Target { ALL, ANY, INDIVIDUAL };
	Target GetType() { return _target; }
	std::vector<obs_scene_item *> GetSceneItems(SceneSelection &s);
	std::string ToString();

private:
	OBSWeakSource _sceneItem;
	Target _target = Target::ALL;
	int _idx = 0;
	friend class SceneItemSelectionWidget;
};

class SceneItemSelectionWidget : public QWidget {
	Q_OBJECT

public:
	enum class AllSelectionType { ALL, ANY };
	SceneItemSelectionWidget(
		QWidget *parent, bool allSelection = true,
		AllSelectionType allType = AllSelectionType::ALL);
	void SetSceneItem(const SceneItemSelection &);
	void SetScene(const SceneSelection &);
	void SetShowAll(bool);
	void SetShowAllSelectionType(AllSelectionType t);
signals:
	void SceneItemChanged(const SceneItemSelection &);

private slots:
	void SceneChanged(const SceneSelection &);
	void SelectionChanged(const QString &name);
	void IdxChanged(int);

private:
	void SetupIdxSelection(int);

	QComboBox *_sceneItems;
	QComboBox *_idx;

	SceneSelection _scene;
	OBSWeakSource _sceneItem;
	bool _showAll = false;
	AllSelectionType _allType = AllSelectionType::ALL;
};
