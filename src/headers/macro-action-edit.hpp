#pragma once

#include <QWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <deque>
#include "macro-entry.hpp"

struct MacroActionInfo {
	using TCreateMethod = std::shared_ptr<MacroAction> (*)();
	using TCreateWidgetMethod = QWidget *(*)();
	TCreateMethod _createFunc;
	TCreateWidgetMethod _createWidgetFunc;
	std::string _name;
};

class MacroActionFactory {
public:
	using TCreateMethod = std::shared_ptr<MacroAction> (*)();

public:
	MacroActionFactory() = delete;
	static bool Register(int id, MacroActionInfo);
	static std::shared_ptr<MacroAction> Create(const int id);
	static QWidget *CreateWidget(const int id);
	static auto GetActionTypes() { return _methods; }

private:
	static std::map<int, MacroActionInfo> _methods;
};

class MacroActionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionEdit(MacroAction *entryData = nullptr);
	void UpdateEntryData();

private slots:
	void ActionSelectionChanged(int idx);

protected:
	QComboBox *_actionSelection;
	QVBoxLayout *_actionWidgetLayout;
	QVBoxLayout *_groupLayout;
	QGroupBox *_group;

	MacroAction *_entryData;

private:
	bool _loading = true;
};
