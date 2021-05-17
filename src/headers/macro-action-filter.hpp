#pragma once
#include <QSpinBox>
#include "macro-action-edit.hpp"

enum class FilterAction {
	ENABLE,
	DISABLE,
};

class MacroActionFilter : public MacroAction {
public:
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroAction> Create()
	{
		return std::make_shared<MacroActionFilter>();
	}

	OBSWeakSource _source;
	OBSWeakSource _filter;
	FilterAction _action = FilterAction::ENABLE;

private:
	static bool _registered;
	static const int id;
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

protected:
	QComboBox *_sources;
	QComboBox *_filters;
	QComboBox *_actions;
	std::shared_ptr<MacroActionFilter> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
