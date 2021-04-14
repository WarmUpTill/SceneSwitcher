#pragma once

#include "advanced-scene-switcher.hpp"
#include "macro-entry.hpp"
#include "macro-condition-scene.hpp"
#include "utility.hpp"

#include <QGroupBox>

struct MacroConditionInfo {
	using TCreateMethod = std::shared_ptr<MacroCondition> (*)();
	using TCreateWidgetMethod = QWidget *(*)();
	TCreateMethod _createFunc;
	TCreateWidgetMethod _createWidgetFunc;
	std::string _name;
};

class MacroConditionFactory {
public:
	using TCreateMethod = std::shared_ptr<MacroCondition> (*)();

public:
	MacroConditionFactory() = delete;
	static bool Register(int id, MacroConditionInfo);
	static std::shared_ptr<MacroCondition> Create(const int id);
	static QWidget *CreateWidget(const int id);
	static auto GetConditionTypes() { return _methods; }

private:
	static std::map<int, MacroConditionInfo> _methods;
};

class MacroConditionEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionEdit(MacroCondition *entryData = nullptr,
			   bool root = true);
	bool IsRootNode();
	void UpdateEntryData();

	static bool enableAdvancedLogic;

private slots:
	void LogicSelectionChanged(int idx);
	void ConditionSelectionChanged(int idx);
	void ExtendClicked();
	void ReduceClicked();

protected:
	QComboBox *_logicSelection;
	QComboBox *_conditionSelection;
	QVBoxLayout *_conditionWidgetLayout;
	QVBoxLayout *_groupLayout;
	QGroupBox *_group;
	QVBoxLayout *_childLayout;
	QPushButton *_extend;
	QPushButton *_reduce;

	MacroCondition *_entryData;

private:
	bool _isRoot = true;
	bool _loading = true;
};
