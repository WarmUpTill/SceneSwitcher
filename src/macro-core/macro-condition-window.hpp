#pragma once
#include "macro.hpp"

#include <QComboBox>
#include <QCheckBox>

namespace advss {

class MacroConditionWindow : public MacroCondition {
public:
	MacroConditionWindow(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionWindow>(m);
	}

private:
	bool
	CheckWindowTitleSwitchDirect(const std::string &currentWindowTitle);
	bool
	CheckWindowTitleSwitchRegex(const std::string &currentWindowTitle,
				    const std::vector<std::string> &windowList);

public:
	std::string _window;
	bool _fullscreen = false;
	bool _maximized = false;
	bool _focus = true;
	bool _windowFocusChanged = false;

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
	void FocusedChanged(int state);
	void WindowFocusChanged(int state);
	void UpdateFocusWindow();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_windowSelection;
	QCheckBox *_fullscreen;
	QCheckBox *_maximized;
	QCheckBox *_focused;
	QCheckBox *_windowFocusChanged;
	QLabel *_focusWindow;
	QHBoxLayout *_focusLayout;
	QTimer _timer;
	std::shared_ptr<MacroConditionWindow> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
