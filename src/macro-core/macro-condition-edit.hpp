#pragma once
#include "macro-condition.hpp"
#include "filter-combo-box.hpp"

#include <memory>

namespace advss {

struct MacroConditionInfo {
	using TCreateMethod = std::shared_ptr<MacroCondition> (*)(Macro *m);
	using TCreateWidgetMethod =
		QWidget *(*)(QWidget *parent, std::shared_ptr<MacroCondition>);
	TCreateMethod _createFunc = nullptr;
	TCreateWidgetMethod _createWidgetFunc = nullptr;
	std::string _name;
	bool _useDurationModifier = true;
};

class MacroConditionFactory {
public:
	MacroConditionFactory() = delete;
	static bool Register(const std::string &, MacroConditionInfo);
	static std::shared_ptr<MacroCondition> Create(const std::string &,
						      Macro *m);
	static QWidget *CreateWidget(const std::string &id, QWidget *parent,
				     std::shared_ptr<MacroCondition>);
	static auto GetConditionTypes() { return GetMap(); }
	static std::string GetConditionName(const std::string &);
	static std::string GetIdByName(const QString &name);
	static bool UsesDurationModifier(const std::string &id);

private:
	static std::map<std::string, MacroConditionInfo> &GetMap();
};

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
	MacroConditionEdit(QWidget *parent = nullptr,
			   std::shared_ptr<MacroCondition> * = nullptr,
			   const std::string &id = "scene", bool root = true);
	bool IsRootNode();
	void SetRootNode(bool);
	void UpdateEntryData(const std::string &id);
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
