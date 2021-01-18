#pragma once

#include <vector>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <obs.hpp>

enum class AdvanceCondition {
	Count,
	Time,
	Random,
};

struct SceneGroup {
	OBSWeakSource getNextScene();
	OBSWeakSource getNextSceneCount();
	OBSWeakSource getNextSceneTime();
	OBSWeakSource getNextSceneRandom();
	void advanceIdx();

	AdvanceCondition type = AdvanceCondition::Count;
	std::vector<OBSWeakSource> scenes = {};

	std::string name = "no-name";
	int count = 1;
	double time = 0;
	bool repeat = false;

	size_t currentIdx = 0;

	int currentCount = -1;
	std::chrono::high_resolution_clock::time_point lastAdvTime;
	OBSWeakSource lastRandomScene = nullptr;

	inline SceneGroup(){};
	inline SceneGroup(std::string name_) : name(name_){};
	inline SceneGroup(std::string name_, AdvanceCondition type_,
			  std::vector<OBSWeakSource> scenes_, int count_,
			  double time_, bool repeat_)
		: name(name_),
		  type(type_),
		  scenes(scenes_),
		  count(count_),
		  time(time_),
		  repeat(repeat_)
	{
	}
};

enum class SwitchTargetType {
	Scene,
	SceneGroup,
};

struct SwitchTarget {
	SwitchTargetType type = SwitchTargetType::Scene;

	OBSWeakSource scene = nullptr;
	bool usePreviousScene = false;
	SceneGroup group;
};

class SceneGroupEditWidget : public QWidget {
	Q_OBJECT

public:
	SceneGroupEditWidget();
	void SetEditSceneGroup(SceneGroup *sg);
	void ShowCurrentTypeEdit();

private slots:
	void TypeChanged(int type);
	void CountChanged(int count);
	void TimeChanged(double time);
	void RepeatChanged(int state);

private:
	QComboBox *type;

	QWidget *timeEdit;
	QWidget *countEdit;

	QSpinBox *count;
	QDoubleSpinBox *time;
	QLabel *random;

	QCheckBox *repeat;

	SceneGroup *sceneGroup = nullptr;
};

// Based on OBS's NameDialog
class SGNameDialog : public QDialog {
	Q_OBJECT

public:
	SGNameDialog(QWidget *parent);

	// Returns true if user clicks OK, false otherwise
	// userTextInput returns string that user typed into dialog
	static bool AskForName(QWidget *parent, const QString &title,
			       const QString &text, std::string &userTextInput,
			       const QString &placeHolder = QString(""),
			       int maxSize = 170);

private:
	QLabel *label;
	QLineEdit *userText;
};
