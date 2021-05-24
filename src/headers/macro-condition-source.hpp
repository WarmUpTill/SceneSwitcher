#pragma once
#include "macro.hpp"
#include <limits>
#include <QWidget>
#include <QComboBox>

enum class SourceCondition {
	ACTIVE,
	SHOWING,
	SETTINGS,
};

class MacroConditionSource : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionSource>();
	}

	OBSWeakSource _source = nullptr;
	SourceCondition _condition = SourceCondition::ACTIVE;
	std::string _settings = "";
	bool _regex = false;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionSourceEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionSourceEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionSource> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionSourceEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionSource>(cond));
	}

private slots:
	void SourceChanged(const QString &text);
	void ConditionChanged(int cond);
	void GetSettingsClicked();
	void SettingsChanged();
	void RegexChanged(int);

protected:
	QComboBox *_sources;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	QPlainTextEdit *_settings;
	QCheckBox *_regex;

	std::shared_ptr<MacroConditionSource> _entryData;

private:
	void SetSettingsSelectionVisible(bool visible);
	bool _loading = true;
};
