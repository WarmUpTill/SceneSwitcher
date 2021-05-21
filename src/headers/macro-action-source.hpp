#pragma once
#include <QSpinBox>
#include "macro-action-edit.hpp"

enum class SourceAction {
	ENABLE,
	DISABLE,
};

class MacroActionSource : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionSource>();
	}

	OBSWeakSource _source;
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
	void SourceChanged(const QString &text);
	void ActionChanged(int value);

protected:
	QComboBox *_sources;
	QComboBox *_actions;
	std::shared_ptr<MacroActionSource> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
