#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

#include <QGroupBox>

class MacroActionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionEdit(QWidget *parent = nullptr,
			std::deque<SceneSequenceSwitch> *entryData = nullptr);
	void UpdateEntryData();

private slots:
	void ActionSelectionChanged(int idx);

protected:
	QComboBox *_actionSelection;
	QVBoxLayout *_actionWidgetLayout;
	QVBoxLayout *_groupLayout;
	QGroupBox *_group;

	std::deque<SceneSequenceSwitch> *_entryData;

private:
	bool _loading = true;
};
