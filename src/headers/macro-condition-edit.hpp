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
};

class MacroConditionFactory {
public:
	MacroConditionFactory() = delete;
	static bool Register(int id, MacroConditionInfo);
	static std::shared_ptr<MacroCondition> Create(const int id);
	static QWidget *CreateWidget(const int id, QWidget *parent,
				     std::shared_ptr<MacroCondition>);
	static auto GetConditionTypes() { return _methods; }

private:
	static std::map<int, MacroConditionInfo> _methods;
};

class MacroConditionEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionEdit(QWidget *parent = nullptr,
			   std::shared_ptr<MacroCondition> * = nullptr,
			   int type = 0, bool root = true);
	bool IsRootNode();
	void UpdateEntryData(int type);
	void Collapse(bool collapsed);

private slots:
	void LogicSelectionChanged(int idx);
	void ConditionSelectionChanged(int idx);

protected:
	QComboBox *_logicSelection;
	QComboBox *_conditionSelection;
	Section *_section;

	std::shared_ptr<MacroCondition> *_entryData;

private:
	bool _isRoot = true;
	bool _loading = true;
};
