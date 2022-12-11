#pragma once
#include "macro.hpp"
#include "resizing-text-edit.hpp"
#include "regex-config.hpp"

#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>

enum class SourceCondition {
	ACTIVE,
	SHOWING,
	SETTINGS,
};

class MacroConditionSource : public MacroCondition {
public:
	MacroConditionSource(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionSource>(m);
	}

	OBSWeakSource _source = nullptr;
	SourceCondition _condition = SourceCondition::ACTIVE;
	std::string _settings = "";
	RegexConfig _regex;

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
	void RegexChanged(RegexConfig);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_sources;
	QComboBox *_conditions;
	QPushButton *_getSettings;
	ResizingPlainTextEdit *_settings;
	RegexConfigWidget *_regex;

	std::shared_ptr<MacroConditionSource> _entryData;

private:
	void SetSettingsSelectionVisible(bool visible);
	bool _loading = true;
};
