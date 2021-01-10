#include <vector>
#include <QComboBox>
#include <obs.hpp>

enum class AdvanceCondition {
	Count,
	Time,
	Random,
};

class SceneGroup {
	void AddScene();
	void RemoveScene();
	void UpdateCount();
	void UpdateTime();

	void getNextScene();

private:
	AdvanceCondition condition;
	std::vector<OBSWeakSource> scenes;
};

enum class SwitchTargetType {
	Scene,
	SceneGroup,
};

struct SwitchTarget {
	SwitchTargetType type;
	union {
		OBSWeakSource scene;
		SceneGroup group;
	};
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
