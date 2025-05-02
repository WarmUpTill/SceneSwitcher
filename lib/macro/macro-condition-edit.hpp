#pragma once
#include "macro-condition.hpp"
#include "macro-condition-factory.hpp"
#include "filter-combo-box.hpp"
#include "duration-control.hpp"

#include <memory>

namespace advss {

class DurationModifierEdit : public QWidget {
	Q_OBJECT
public:
	DurationModifierEdit(QWidget *parent = nullptr);
	void SetValue(DurationModifier &value);
	void SetDuration(const Duration &d);

private slots:
	void _ModifierChanged(int value);
	void ToggleClicked();
signals:
	void DurationChanged(const Duration &value);
	void ModifierChanged(DurationModifier::Type value);

private:
	void Collapse(bool collapse);

	DurationSelection *_duration;
	QComboBox *_condition;
	QPushButton *_toggle;
};

class MacroConditionEdit : public MacroSegmentEdit {
	Q_OBJECT

public:
	MacroConditionEdit(
		QWidget *parent = nullptr,
		std::shared_ptr<MacroCondition> * = nullptr,
		const std::string &id = MacroCondition::GetDefaultID().data(),
		bool root = true);
	bool IsRootNode() const;
	void SetRootNode(bool);
	void SetupWidgets(bool basicSetup = false);
	void SetEntryData(std::shared_ptr<MacroCondition> *);

private slots:
	void LogicSelectionChanged(int idx);
	void ConditionSelectionChanged(const QString &text);
	void DurationChanged(const Duration &value);
	void DurationModifierChanged(DurationModifier::Type m);

private:
	void SetLogicSelection();
	std::shared_ptr<MacroSegment> Data() const;

	QComboBox *_logicSelection;
	FilterComboBox *_conditionSelection;
	DurationModifierEdit *_dur;

	std::shared_ptr<MacroCondition> *_entryData;
	bool _isRoot = true;
	bool _loading = true;
};

} // namespace advss
