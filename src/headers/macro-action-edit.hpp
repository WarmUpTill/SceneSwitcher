#pragma once
#include "macro.hpp"

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
	static auto GetActionTypes() { return _methods; }
	static std::string GetActionName(const std::string &id);
	static std::string GetIdByName(const QString &name);

private:
	static std::map<std::string, MacroActionInfo> _methods;
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

private:
	MacroSegment *Data();

	QComboBox *_actionSelection;

	std::shared_ptr<MacroAction> *_entryData;
	bool _loading = true;
};
