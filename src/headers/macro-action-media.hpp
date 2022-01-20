#pragma once
#include "macro-action-edit.hpp"

#include <QSpinBox>
#include <QHBoxLayout>

enum class MediaAction {
	PLAY,
	PAUSE,
	STOP,
	RESTART,
	NEXT,
	PREVIOUS,
};

class MacroActionMedia : public MacroAction {
public:
	MacroActionMedia(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetShortDesc();
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionMedia>(m);
	}

	OBSWeakSource _mediaSource;
	MediaAction _action = MediaAction::PLAY;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionMediaEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionMediaEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionMedia> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionMediaEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionMedia>(action));
	}

private slots:
	void SourceChanged(const QString &text);
	void ActionChanged(int value);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	QComboBox *_mediaSources;
	QComboBox *_actions;
	std::shared_ptr<MacroActionMedia> _entryData;

private:
	QHBoxLayout *_mainLayout;
	bool _loading = true;
};
