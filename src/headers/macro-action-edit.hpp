#pragma once

#include <QWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <deque>
#include "macro-entry.hpp"

class MacroActionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionEdit(MacroAction *entryData = nullptr);
	void UpdateEntryData();

private slots:
	void ActionSelectionChanged(int idx);

protected:
	QComboBox *_actionSelection;
	QVBoxLayout *_actionWidgetLayout;
	QVBoxLayout *_groupLayout;
	QGroupBox *_group;

	MacroAction *_entryData;

private:
	bool _loading = true;
};
