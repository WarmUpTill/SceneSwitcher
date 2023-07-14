#pragma once
#include "macro-action.hpp"
#include "filter-combo-box.hpp"

#include <memory>

namespace advss {

class SwitchButton;

struct MacroActionInfo {
	using TCreateMethod = std::shared_ptr<MacroAction> (*)(Macro *m);
	using TCreateWidgetMethod = QWidget *(*)(QWidget *parent,
						 std::shared_ptr<MacroAction>);
	TCreateMethod _createFunc = nullptr;
	TCreateWidgetMethod _createWidgetFunc = nullptr;
	std::string _name;
};

class MacroActionFactory {
public:
	MacroActionFactory() = delete;

	static bool Register(const std::string &id, MacroActionInfo);
	static std::shared_ptr<MacroAction> Create(const std::string &id,
						   Macro *m);
	static QWidget *CreateWidget(const std::string &id, QWidget *parent,
				     std::shared_ptr<MacroAction> action);
	static auto GetActionTypes() { return GetMap(); }
	static std::string GetActionName(const std::string &id);
	static std::string GetIdByName(const QString &name);

private:
	static std::map<std::string, MacroActionInfo> &GetMap();
};

class MacroActionEdit : public MacroSegmentEdit {
	Q_OBJECT

public:
	MacroActionEdit(QWidget *parent = nullptr,
			std::shared_ptr<MacroAction> * = nullptr,
			const std::string &id = "scene_switch");
	void UpdateEntryData(const std::string &id);
	void SetEntryData(std::shared_ptr<MacroAction> *);

private slots:
	void ActionSelectionChanged(const QString &text);
	void ActionEnableChanged(bool);

private:
	std::shared_ptr<MacroSegment> Data();
	void SetDisableEffect(bool);

	FilterComboBox *_actionSelection;
	SwitchButton *_enable;

	std::shared_ptr<MacroAction> *_entryData;
	bool _loading = true;
};

} // namespace advss
