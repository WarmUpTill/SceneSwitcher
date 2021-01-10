#pragma once

#include <vector>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <obs.hpp>

enum class AdvanceCondition {
	Count,
	Time,
	Random,
};

struct SceneGroup {
	void AddScene();
	void RemoveScene();
	void UpdateCount();
	void UpdateTime();

	void getNextScene();

	AdvanceCondition type = AdvanceCondition::Count;
	std::vector<OBSWeakSource> scenes = {};

	std::string name = "no-name";
	int count = 0;
	double time = 0;

	inline SceneGroup(){};
	inline SceneGroup(std::string name_) : name(name_){};
	inline SceneGroup(std::string name_, AdvanceCondition type_,
			  std::vector<OBSWeakSource> scenes_, int count_,
			  double time_)
		: name(name_),
		  type(type_),
		  scenes(scenes_),
		  count(count_),
		  time(time_)
	{
	}
};

enum class SwitchTargetType {
	Scene,
	SceneGroup,
};

struct SwitchTarget {
	SwitchTargetType type;

	OBSWeakSource scene;
	SceneGroup group;
};

class SceneGroupWidget : public QWidget {
	Q_OBJECT

public:
	SceneGroupWidget(SceneGroup *s);
private slots:
	void ConditionChanged(int cond);
	//...

private:
	QComboBox *scenes;
	QComboBox *condition;

	SceneGroup *group;
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
