#pragma once
#include "macro-action-edit.hpp"
#include "variable-text-edit.hpp"
#include "source-selection.hpp"

#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

namespace advss {

enum class SourceAction {
	ENABLE,
	DISABLE,
	SETTINGS,
	REFRESH_SETTINGS,
	SETTINGS_BUTTON,
};

struct SourceSettingButton {
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string ToString() const;

	std::string id = "";
	std::string description = "";
};

class MacroActionSource : public MacroAction {
public:
	MacroActionSource(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionSource>(m);
	}

	SourceSelection _source;
	SourceSettingButton _button;
	StringVariable _settings = "";
	SourceAction _action = SourceAction::ENABLE;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionSourceEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionSourceEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionSource> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionSourceEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionSource>(action));
	}

private slots:
	void SourceChanged(const SourceSelection &);
	void ActionChanged(int value);
	void ButtonChanged(int idx);
	void GetSettingsClicked();
	void SettingsChanged();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	SourceSelectionWidget *_sources;
	QComboBox *_actions;
	QComboBox *_settingsButtons;
	QPushButton *_getSettings;
	VariableTextEdit *_settings;
	QLabel *_warning;
	std::shared_ptr<MacroActionSource> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
