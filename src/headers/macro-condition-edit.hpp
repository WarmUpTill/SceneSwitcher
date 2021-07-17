#pragma once

#include "advanced-scene-switcher.hpp"
#include "macro.hpp"
#include "macro-condition-scene.hpp"
#include "section.hpp"
#include "macro-entry-controls.hpp"
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
	void SetRootNode(bool);
	void UpdateEntryData(const std::string &id, bool collapse);

private slots:
	void LogicSelectionChanged(int idx);
	void ConditionSelectionChanged(const QString &text);
	void DurationChanged(double seconds);
	void DurationConditionChanged(DurationCondition cond);
	void DurationUnitChanged(DurationUnit unit);
	void HeaderInfoChanged(const QString &);
	void Add();
	void Remove();
	void Up();
	void Down();
signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);
	void AddAt(int idx);
	void RemoveAt(int idx);
	void UpAt(int idx);
	void DownAt(int idx);

protected:
	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);

	QComboBox *_logicSelection;
	QComboBox *_conditionSelection;
	Section *_section;
	QLabel *_headerInfo;
	DurationConstraintEdit *_dur;
	MacroEntryControls *_controls;

	std::shared_ptr<MacroCondition> *_entryData;

private:
	bool _isRoot = true;
	bool _loading = true;
};
