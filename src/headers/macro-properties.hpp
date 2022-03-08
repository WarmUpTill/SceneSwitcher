#pragma once

#include <QWidget>
#include <QDialog>
#include <QCheckBox>
#include <obs-data.h>

class MacroProperties {
public:
	void Save(obs_data_t *obj);
	void Load(obs_data_t *obj);

	bool _highlightExecuted = false;
	bool _highlightConditions = false;
	bool _highlightActions = false;
};

class MacroPropertiesDialog : public QDialog {
	Q_OBJECT

public:
	MacroPropertiesDialog(QWidget *parent, const MacroProperties &);
	static bool AskForSettings(QWidget *parent, MacroProperties &userInput);

private:
	QCheckBox *_executed;
	QCheckBox *_conditions;
	QCheckBox *_actions;
};
