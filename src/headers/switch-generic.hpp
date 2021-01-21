#pragma once
#include <obs.hpp>
#include <QComboBox>

struct SceneSwitcherEntry {
	SwitchTargetType targetType = SwitchTargetType::Scene;
	SceneGroup group;
	OBSWeakSource scene = nullptr;
	OBSWeakSource transition = nullptr;
	bool usePreviousScene = false;

	virtual const char *getType() = 0;
	virtual bool initialized();
	virtual bool valid();
	virtual void logMatch();
	virtual OBSWeakSource getScene();

	inline SceneSwitcherEntry() {}

	inline SceneSwitcherEntry(OBSWeakSource scene_,
				  OBSWeakSource transition_,
				  bool usePreviousScene_ = false)
		: scene(scene_),
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
		     bool usePreviousScene = true,
		     bool addSceneGroup = false);
	virtual SceneSwitcherEntry *getSwitchData();
	virtual void setSwitchData(SceneSwitcherEntry *s);

	static void swapSwitchData(SwitchWidget *s1, SwitchWidget *s2);

private slots:
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
