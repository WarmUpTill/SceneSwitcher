#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

#include <QGroupBox>

class SwitchEntryActionEdit : public QWidget {
	Q_OBJECT

public:
	SwitchEntryActionEdit(QWidget *parent = nullptr,
			      SceneSequenceSwitch *entryData = nullptr);
	void UpdateEntryData();

private slots:
	void ActionSelectionChanged(int idx);
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
	bool _loading = true;
};
