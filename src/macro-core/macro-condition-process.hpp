#pragma once
#include "macro.hpp"

#include <QComboBox>
#include <QCheckBox>

class MacroConditionProcess : public MacroCondition {
public:
	MacroConditionProcess(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionProcess>(m);
	}

	std::string _process;
	bool _focus = true;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionProcessEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionProcessEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionProcess> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionProcessEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionProcess>(cond));
	}

private slots:
	void ProcessChanged(const QString &text);
	void FocusChanged(int state);
	void UpdateFocusProcess();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_processSelection;
	QCheckBox *_focused;
	QLabel *_focusProcess;
	QHBoxLayout *_focusLayout;
	QTimer _timer;
	std::shared_ptr<MacroConditionProcess> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
