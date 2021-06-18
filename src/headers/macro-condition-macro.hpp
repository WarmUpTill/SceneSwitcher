#pragma once
#include "macro.hpp"
#include "macro-selection.hpp"
#include <QWidget>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>

enum class MacroConditionMacroType {
	COUNT,
	STATE,
};

enum class CounterCondition {
	BELOW,
	ABOVE,
	EQUAL,
};

class MacroConditionMacro : public MacroRefCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionMacro>();
	}

	MacroConditionMacroType _type = MacroConditionMacroType::STATE;
	CounterCondition _counterCondition = CounterCondition::BELOW;
	int _count = 0;

private:
	bool CheckCountCondition();
	bool CheckStateCondition();

	static bool _registered;
	static const std::string id;
};

class MacroConditionMacroEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionMacroEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionMacro> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionMacroEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionMacro>(cond));
	}

private slots:
	void MacroChanged(const QString &text);
	void MacroRemove(const QString &name);
	void TypeChanged(int type);
	void CountChanged(int value);
	void ConditionChanged(int cond);
	void ResetClicked();
	void UpdateCount();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	MacroSelection *_macros;
	QComboBox *_types;
	QComboBox *_counterConditions;
	QSpinBox *_count;
	QLabel *_currentCount;
	QPushButton *_resetCount;
	QHBoxLayout *_settingsLine1;
	QHBoxLayout *_settingsLine2;
	std::unique_ptr<QTimer> _timer;
	std::shared_ptr<MacroConditionMacro> _entryData;

private:
	void ClearLayouts();
	void SetupStateWidgets();
	void SetupCountWidgets();
	void ResetTimer();
	bool _loading = true;
};
