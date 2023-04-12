#pragma once
#include <obs.hpp>
#include <QComboBox>

#include "scene-group.hpp"

namespace advss {

enum class SwitchTargetType {
	Scene,
	SceneGroup,
};

struct SceneSwitcherEntry {
	SwitchTargetType targetType = SwitchTargetType::Scene;
	SceneGroup *group = nullptr;
	OBSWeakSource scene = nullptr;
	OBSWeakSource transition = nullptr;
	bool usePreviousScene = false;
	bool useCurrentTransition = false;

	virtual const char *getType() = 0;
	virtual bool initialized();
	virtual bool valid();
	virtual void logMatchScene();
	virtual void logMatchSceneGroup();
	virtual void logMatch();
	virtual OBSWeakSource getScene();
	virtual void save(obs_data_t *obj,
			  const char *targetTypeSaveName = "targetType",
			  const char *targetSaveName = "target",
			  const char *transitionSaveName = "transition");
	virtual void load(obs_data_t *obj,
			  const char *targetTypeLoadName = "targetType",
			  const char *targetLoadName = "target",
			  const char *transitionLoadName = "transition");

	inline SceneSwitcherEntry() {}

	inline SceneSwitcherEntry(OBSWeakSource scene_,
				  OBSWeakSource transition_,
				  bool usePreviousScene_ = false)
		: scene(scene_),
		  transition(transition_),
		  usePreviousScene(usePreviousScene_)
	{
	}

	inline SceneSwitcherEntry(SceneGroup *group_, OBSWeakSource transition_,
				  bool usePreviousScene_ = false)
		: group(group_),
		  transition(transition_),
		  usePreviousScene(usePreviousScene_)
	{
	}

	inline SceneSwitcherEntry(SwitchTargetType targetType_,
				  SceneGroup *group_, OBSWeakSource scene_,
				  OBSWeakSource transition_,
				  bool usePreviousScene_ = false)
		: targetType(targetType_),
		  group(group_),
		  scene(scene_),
		  transition(transition_),
		  usePreviousScene(usePreviousScene_)
	{
	}

	virtual ~SceneSwitcherEntry() {}
};

class SwitchWidget : public QWidget {
	Q_OBJECT

public:
	SwitchWidget(QWidget *parent, SceneSwitcherEntry *s,
		     bool usePreviousScene = true, bool addSceneGroup = false,
		     bool addCurrentTransition = true);
	virtual SceneSwitcherEntry *getSwitchData();
	virtual void setSwitchData(SceneSwitcherEntry *s);
	void showSwitchData();

	static void swapSwitchData(SwitchWidget *s1, SwitchWidget *s2);

protected slots:
	void SceneChanged(const QString &text);
	void TransitionChanged(const QString &text);
	void SceneGroupAdd(const QString &name);
	void SceneGroupRemove(const QString &name);
	void SceneGroupRename(const QString &oldName, const QString &newName);

protected:
	bool loading = true;

	QComboBox *scenes;
	QComboBox *transitions;

	SceneSwitcherEntry *switchData;
};

} // namespace advss
