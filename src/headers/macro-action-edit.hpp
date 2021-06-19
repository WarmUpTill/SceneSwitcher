#pragma once

#include <QWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
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
	static bool Register(const std::string &id, MacroActionInfo);
	static std::shared_ptr<MacroAction> Create(const std::string &id);
	static QWidget *CreateWidget(const std::string &id, QWidget *parent,
				     std::shared_ptr<MacroAction> action);
	static auto GetActionTypes() { return _methods; }
	static std::string GetActionName(const std::string &id);
	static std::string GetIdByName(const QString &name);

private:
	static std::map<std::string, MacroActionInfo> _methods;
};

class MacroActionEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionEdit(QWidget *parent = nullptr,
			std::shared_ptr<MacroAction> * = nullptr,
			const std::string &id = "scene_switch",
			bool startCollapsed = false);
	void UpdateEntryData(const std::string &id, bool collapse);

private slots:
	void ActionSelectionChanged(const QString &text);
	void HeaderInfoChanged(const QString &);
signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);

protected:
	QComboBox *_actionSelection;
	Section *_section;
	QLabel *_headerInfo;

	std::shared_ptr<MacroAction> *_entryData;

private:
	bool _loading = true;
};
