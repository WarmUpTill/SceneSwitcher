#pragma once
#include <obs.hpp>
#include <QComboBox>

struct SceneSwitcherEntry {
	SwitchTarget target;
	OBSWeakSource scene = nullptr;
	OBSWeakSource transition = nullptr;
	bool usePreviousScene = false;

	virtual const char *getType() = 0;
	virtual bool initialized();
	virtual bool valid();
	virtual void logMatch();

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

class TargetSelection : public QComboBox {
	Q_OBJECT
private slots:
	void SceneGroupRename(const QString &oldName, const QString newName)
	{
		bool renameSelected = currentText() == oldName;
		int idx = findText(oldName);

		if (idx == -1) {
			return;
		}

		removeItem(idx);
		insertItem(idx, newName);

		if (renameSelected)
			setCurrentIndex(findText(newName));
	}
};

class SwitchWidget : public QWidget {
	Q_OBJECT

public:
	SwitchWidget(SceneSwitcherEntry *s, bool usePreviousScene = true);
	virtual SceneSwitcherEntry *getSwitchData();
	virtual void setSwitchData(SceneSwitcherEntry *s);

	static void swapSwitchData(SwitchWidget *s1, SwitchWidget *s2);

private slots:
	void SceneChanged(const QString &text);
	void TransitionChanged(const QString &text);

protected:
	bool loading = true;

	QComboBox *scenes;
	QComboBox *transitions;

	SceneSwitcherEntry *switchData;
};
