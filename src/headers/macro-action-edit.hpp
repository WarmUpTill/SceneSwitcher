#pragma once

#include <QWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <deque>
#include "macro.hpp"
#include "section.hpp"

struct MacroActionInfo {
	using TCreateMethod = std::shared_ptr<MacroAction> (*)();
	using TCreateWidgetMethod = QWidget *(*)(QWidget *parent,
						 std::shared_ptr<MacroAction>);
	TCreateMethod _createFunc;
	TCreateWidgetMethod _createWidgetFunc;
	std::string _name;
};

class MacroActionFactory {
public:
	MacroActionFactory() = delete;
	static bool Register(int id, MacroActionInfo);
	static std::shared_ptr<MacroAction> Create(const int id);
	static QWidget *CreateWidget(const int id, QWidget *parent,
				     std::shared_ptr<MacroAction> action);
	static auto GetActionTypes() { return _methods; }
	static std::string GetActionName(int id);

private:
	static std::map<int, MacroActionInfo> _methods;
};

class MacroActionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionEdit(QWidget *parent = nullptr,
			std::shared_ptr<MacroAction> * = nullptr, int type = 0,
			bool startCollapsed = false);
	void UpdateEntryData(int type);
	void Collapse(bool collapsed);

private slots:
	void ActionSelectionChanged(int idx);

protected:
	QComboBox *_actionSelection;
	Section *_section;

	std::shared_ptr<MacroAction> *_entryData;

private:
	bool _loading = true;
};
