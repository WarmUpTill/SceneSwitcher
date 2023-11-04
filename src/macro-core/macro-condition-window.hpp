#pragma once
#include "macro-condition-edit.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"

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

	StringVariable _window;
	RegexConfig _windowRegex;
	bool _checkTitle = true;
	bool _fullscreen = false;
	bool _maximized = false;
	bool _focus = true;
	bool _windowFocusChanged = false;

	// For now only supported on Windows
	bool _checkText = false;
	StringVariable _text;
	RegexConfig _textRegex = RegexConfig::PartialMatchRegexConfig();

private:
	bool WindowMatchesRequirements(const std::string &window) const;
	bool WindowMatches(const std::vector<std::string> &windowList);
	bool WindowRegexMatches(const std::vector<std::string> &windowList);
	void SetVariableValueBasedOnMatch(const std::string &matchWindow);
	void SetupTempVars();

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
	void WindowRegexChanged(RegexConfig);
	void CheckTitleChanged(int state);
	void FullscreenChanged(int state);
	void MaximizedChanged(int state);
	void FocusedChanged(int state);
	void WindowFocusChanged(int state);
	void CheckTextChanged(int state);
	void WindowTextChanged();
	void TextRegexChanged(RegexConfig);
	void UpdateFocusWindow();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_windowSelection;
	RegexConfigWidget *_windowRegex;
	QCheckBox *_checkTitle;
	QCheckBox *_fullscreen;
	QCheckBox *_maximized;
	QCheckBox *_focused;
	QCheckBox *_windowFocusChanged;
	QCheckBox *_checkText;
	VariableTextEdit *_text;
	RegexConfigWidget *_textRegex;
	QLabel *_focusWindow;
	QHBoxLayout *_currentFocusLayout;
	QTimer _timer;
	std::shared_ptr<MacroConditionWindow> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
