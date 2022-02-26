#pragma once
#include "macro-action-edit.hpp"
#include "resizing-text-edit.hpp"

#include <QSpinBox>

enum class FilterAction {
	ENABLE,
	DISABLE,
	SETTINGS,
};

class MacroActionFilter : public MacroAction {
public:
	MacroActionFilter(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionFilter>(m);
	}

	OBSWeakSource _source;
	OBSWeakSource _filter;
	FilterAction _action = FilterAction::ENABLE;
	std::string _settings = "";

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionFilterEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionFilterEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionFilter> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionFilterEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionFilter>(action));
	}

private slots:
	void SourceChanged(const QString &text);
	void FilterChanged(const QString &text);
	void ActionChanged(int value);
	void GetSettingsClicked();
	void SettingsChanged();
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_sources;
	QComboBox *_filters;
	QComboBox *_actions;
	QPushButton *_getSettings;
	ResizingPlainTextEdit *_settings;
	std::shared_ptr<MacroActionFilter> _entryData;

private:
	void SetWidgetVisibility(bool);
	bool _loading = true;
};
