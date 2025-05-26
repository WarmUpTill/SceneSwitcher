#pragma once
#include "macro-condition-edit.hpp"
#include "regex-config.hpp"
#include "variable-string.hpp"

#include <QCheckBox>

namespace advss {

class MacroConditionProcess : public MacroCondition {
public:
	MacroConditionProcess(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionProcess>(m);
	}

	StringVariable _process;
	bool _checkFocus = true;
	RegexConfig _regex = RegexConfig::PartialMatchRegexConfig();

private:
	void SetupTempVars();

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

protected:
	void showEvent(QShowEvent *event) override;

private slots:
	void ProcessChanged(const QString &text);
	void RegexChanged(const RegexConfig &);
	void FocusChanged(int state);
	void UpdateFocusProcess();
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	QComboBox *_processSelection;
	RegexConfigWidget *_regex;
	QCheckBox *_focused;
	QLabel *_focusProcess;
	QHBoxLayout *_focusLayout;
	QTimer _timer;
	std::shared_ptr<MacroConditionProcess> _entryData;
	bool _loading = true;
};

} // namespace advss
