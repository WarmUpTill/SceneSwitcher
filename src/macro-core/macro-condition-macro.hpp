#pragma once
#include "macro-condition-edit.hpp"
#include "macro-selection.hpp"
#include "macro-list.hpp"
#include "macro-segment-selection.hpp"

#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QListWidget>

namespace advss {

class MacroConditionMacro : public MultiMacroRefCondtition,
			    public MacroRefCondition {
public:
	MacroConditionMacro(Macro *m)
		: MacroCondition(m),
		  MultiMacroRefCondtition(m),
		  MacroRefCondition(m)
	{
	}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool PostLoad() override;
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionMacro>(m);
	}

	enum class Type {
		COUNT,
		STATE,
		MULTI_STATE,
		ACTION_DISABLED,
		ACTION_ENABLED,
	};
	Type _type = Type::STATE;

	enum class CounterCondition {
		BELOW,
		ABOVE,
		EQUAL,
	};
	CounterCondition _counterCondition = CounterCondition::BELOW;
	NumberVariable<int> _count = 0;

	enum class MultiStateCondition {
		BELOW,
		EQUAL,
		ABOVE,
	};
	MultiStateCondition _multiSateCondition = MultiStateCondition::ABOVE;
	NumberVariable<int> _multiSateCount = 0;
	IntVariable _actionIndex = 1;

private:
	bool CheckCountCondition();
	bool CheckStateCondition();
	bool CheckMultiStateCondition();
	bool CheckActionStateCondition();

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
	void CountChanged(const NumberVariable<int> &value);
	void CountConditionChanged(int cond);
	void ResetClicked();
	void UpdateCount();
	void UpdatePaused();
	void MultiStateConditionChanged(int cond);
	void MultiStateCountChanged(const NumberVariable<int> &value);
	void Add(const std::string &);
	void Remove(int);
	void Replace(int, const std::string &);
	void ActionIndexChanged(const IntVariable &value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	MacroSelection *_macros;
	QComboBox *_types;
	QComboBox *_counterConditions;
	VariableSpinBox *_count;
	QLabel *_currentCount;
	QLabel *_pausedWarning;
	QPushButton *_resetCount;
	QHBoxLayout *_settingsLine1;
	QHBoxLayout *_settingsLine2;
	MacroList *_macroList;
	QComboBox *_multiStateConditions;
	VariableSpinBox *_multiStateCount;
	MacroSegmentSelection *_actionIndex;
	QTimer _countTimer;
	QTimer _pausedTimer;
	std::shared_ptr<MacroConditionMacro> _entryData;

private:
	void ClearLayouts();
	void SetupWidgets();
	void SetupStateWidgets();
	void SetupMultiStateWidgets();
	void SetupCountWidgets();
	void SetupActionStateWidgets(bool enable);
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
