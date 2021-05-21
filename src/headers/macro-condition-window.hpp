#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionWindow : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionWindow>();
	}

private:
	bool CheckWindowTitleSwitchDirect(std::string &currentWindowTitle);
	bool CheckWindowTitleSwitchRegex(std::string &currentWindowTitle,
					 std::vector<std::string> &windowList);

public:
	std::string _window;
	bool _fullscreen = false;
	bool _maximized = false;
	bool _focus = true;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionWindowEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionWindowEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionWindow> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionWindowEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionWindow>(cond));
	}

private slots:
	void WindowChanged(const QString &text);
	void FullscreenChanged(int state);
	void MaximizedChanged(int state);
	void FocusChanged(int state);

protected:
	QComboBox *_windowSelection;
	QCheckBox *_fullscreen;
	QCheckBox *_maximized;
	QCheckBox *_focused;
	std::shared_ptr<MacroConditionWindow> _entryData;

private:
	bool _loading = true;
};
