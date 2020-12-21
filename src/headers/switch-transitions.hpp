#pragma once
#include "switch-generic.hpp"

struct SceneTransition : SceneSwitcherEntry {
	OBSWeakSource scene2 = nullptr;

	const char *getType() { return "transition"; }
	bool initialized();
	bool valid();

	inline SceneTransition(){};
	inline SceneTransition(OBSWeakSource scene_, OBSWeakSource scene2_,
			       OBSWeakSource transition_)
		: SceneSwitcherEntry(scene_, transition_), scene2(scene2_)
	{
	}
};

struct DefaultSceneTransition : SceneSwitcherEntry {

	const char *getType() { return "def_transition"; }

	inline DefaultSceneTransition(){};
	inline DefaultSceneTransition(OBSWeakSource scene_,
				      OBSWeakSource transition_)
		: SceneSwitcherEntry(scene_, transition_)
	{
	}
};

class TransitionSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	TransitionSwitchWidget(SceneTransition *s);
	SceneTransition *getSwitchData();
	void setSwitchData(SceneTransition *s);

	static void swapSwitchData(TransitionSwitchWidget *s1,
				   TransitionSwitchWidget *s2);

private slots:
	void Scene2Changed(const QString &text);

private:
	QComboBox *scenes2;

	SceneTransition *switchData;
};

class DefTransitionSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	DefTransitionSwitchWidget(DefaultSceneTransition *s);
	DefaultSceneTransition *getSwitchData();
	void setSwitchData(DefaultSceneTransition *s);

	static void swapSwitchData(DefTransitionSwitchWidget *s1,
				   DefTransitionSwitchWidget *s2);

private:
	DefaultSceneTransition *switchData;
};
