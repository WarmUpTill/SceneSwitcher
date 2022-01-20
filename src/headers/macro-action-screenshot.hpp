#pragma once
#include "macro-action-edit.hpp"

class MacroActionScreenshot : public MacroAction {
public:
	MacroActionScreenshot(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionScreenshot>(m);
	}
	OBSWeakSource _source;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionScreenshotEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionScreenshotEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionScreenshot> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionScreenshotEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionScreenshot>(
				action));
	}
private slots:
	void SourceChanged(const QString &text);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_sources;
	std::shared_ptr<MacroActionScreenshot> _entryData;

private:
	bool _loading = true;
};
