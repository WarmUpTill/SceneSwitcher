#pragma once
#include <QDoubleSpinBox>

#include "switch-generic.hpp"

struct SceneTransition : SceneSwitcherEntry {
	OBSWeakSource scene2 = nullptr;
	double duration = 0.;

	const char *getType() { return "transition"; }
	bool initialized();
	bool valid();
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);

	inline SceneTransition(){};
	inline SceneTransition(OBSWeakSource scene_, OBSWeakSource scene2_,
			       OBSWeakSource transition_, double duration_)
		: SceneSwitcherEntry(scene_, transition_),
		  scene2(scene2_),
		  duration(duration_)
	{
	}
};

struct DefaultSceneTransition : SceneSwitcherEntry {
	static bool pause;
	static unsigned int delay;

	const char *getType() { return "def_transition"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
	bool checkMatch(OBSWeakSource currentScene);
	void setTransition();

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
	TransitionSwitchWidget(QWidget *parent, SceneTransition *s);
	SceneTransition *getSwitchData();
	void setSwitchData(SceneTransition *s);

	static void swapSwitchData(TransitionSwitchWidget *s1,
				   TransitionSwitchWidget *s2);

private slots:
	void Scene2Changed(const QString &text);
	void DurationChanged(double dur);

private:
	QComboBox *scenes2;
	QDoubleSpinBox *duration;

	SceneTransition *switchData;
};

class DefTransitionSwitchWidget : public SwitchWidget {
	Q_OBJECT

public:
	DefTransitionSwitchWidget(QWidget *parent, DefaultSceneTransition *s);
	DefaultSceneTransition *getSwitchData();
	void setSwitchData(DefaultSceneTransition *s);

	static void swapSwitchData(DefTransitionSwitchWidget *s1,
				   DefTransitionSwitchWidget *s2);

private:
	DefaultSceneTransition *switchData;
};
