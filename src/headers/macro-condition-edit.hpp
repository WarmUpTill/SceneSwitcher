#pragma once

#include "advanced-scene-switcher.hpp"
#include "macro.hpp"
#include "macro-condition-scene.hpp"
#include "section.hpp"
#include "utility.hpp"

#include <QGroupBox>

struct MacroConditionInfo {
	using TCreateMethod = std::shared_ptr<MacroCondition> (*)();
	using TCreateWidgetMethod =
		QWidget *(*)(QWidget *parent, std::shared_ptr<MacroCondition>);
	TCreateMethod _createFunc;
	TCreateWidgetMethod _createWidgetFunc;
	std::string _name;
	bool _useDurationConstraint = true;
};

class MacroConditionFactory {
public:
	MacroConditionFactory() = delete;
	static bool Register(const std::string &, MacroConditionInfo);
	static std::shared_ptr<MacroCondition> Create(const std::string &);
	static QWidget *CreateWidget(const std::string &id, QWidget *parent,
				     std::shared_ptr<MacroCondition>);
	static auto GetConditionTypes() { return _methods; }
	static std::string GetConditionName(const std::string &);
	static std::string GetIdByName(const QString &name);
	static bool UsesDurationConstraint(const std::string &id);

private:
	static std::map<std::string, MacroConditionInfo> _methods;
};

class MacroConditionEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionEdit(QWidget *parent = nullptr,
			   std::shared_ptr<MacroCondition> * = nullptr,
			   const std::string &id = "scene", bool root = true,
			   bool startCollapsed = false);
	bool IsRootNode();
	void UpdateEntryData(const std::string &id, bool collapse);

private slots:
	void LogicSelectionChanged(int idx);
	void ConditionSelectionChanged(const QString &text);
	void DurationChanged(double seconds);
	void DurationConditionChanged(DurationCondition cond);
	void DurationUnitChanged(DurationUnit unit);
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_logicSelection;
	QComboBox *_conditionSelection;
	Section *_section;
	QLabel *_headerInfo;
	DurationConstraintEdit *_dur;

	std::shared_ptr<MacroCondition> *_entryData;

private:
	bool _isRoot = true;
	bool _loading = true;
};
