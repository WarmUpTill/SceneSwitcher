#pragma once
#include "macro.hpp"
#include "macro-selection.hpp"
#include "macro-list.hpp"

#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QListWidget>

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
	};
	Type _type = Type::STATE;

	enum class CounterCondition {
		BELOW,
		ABOVE,
		EQUAL,
	};
	CounterCondition _counterCondition = CounterCondition::BELOW;
	int _count = 0;

	enum class MultiStateCondition {
		BELOW,
		EQUAL,
		ABOVE,
	};
	MultiStateCondition _multiSateCondition = MultiStateCondition::ABOVE;
	int _multiSateCount = 0;

private:
	bool CheckCountCondition();
	bool CheckStateCondition();
	bool CheckMultiStateCondition();

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
	void CountConditionChanged(int cond);
	void ResetClicked();
	void UpdateCount();
	void UpdatePaused();
	void MultiStateConditionChanged(int cond);
	void MultiStateCountChanged(int value);
	void Add(const std::string &);
	void Remove(int);
	void Replace(int, const std::string &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	MacroSelection *_macros;
	QComboBox *_types;
	QComboBox *_counterConditions;
	QSpinBox *_count;
	QLabel *_currentCount;
	QLabel *_pausedWarning;
	QPushButton *_resetCount;
	QHBoxLayout *_settingsLine1;
	QHBoxLayout *_settingsLine2;
	MacroList *_macroList;
	QComboBox *_multiStateConditions;
	QSpinBox *_multiStateCount;
	QTimer _countTimer;
	QTimer _pausedTimer;
	std::shared_ptr<MacroConditionMacro> _entryData;

private:
	void ClearLayouts();
	void SetupStateWidgets();
	void SetupMultiStateWidgets();
	void SetupCountWidgets();
	void SetWidgetVisibility();
	bool _loading = true;
};
