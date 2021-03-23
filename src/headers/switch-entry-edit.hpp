#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

#include <QGroupBox>

class SwitchEntryEdit : public QWidget {
	Q_OBJECT

public:
	SwitchEntryEdit(QWidget *parent = nullptr,
			SceneSequenceSwitch *entryData = nullptr);
	bool IsRootNode();
	void UpdateEntryData();

	static bool enableAdvancedLogic;

private slots:
	void LogicSelectionChanged(int idx);
	void ConditionSelectionChanged(int idx);
	void ExtendClicked();
	void ReduceClicked();

protected:
	QComboBox *_logicSelection;
	QComboBox *_conditionSelection;
	QVBoxLayout *_conditionLayout;
	QVBoxLayout *_groupLayout;
	QGroupBox *_group;
	QVBoxLayout *_childLayout;
	QPushButton *_extend;
	QPushButton *_reduce;

	SceneSequenceSwitch *_entryData;

private:
	bool _isRoot = true;
	bool _loading = true;
};
