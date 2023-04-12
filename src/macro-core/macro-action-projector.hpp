#pragma once
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "source-selection.hpp"

namespace advss {

class MacroActionProjector : public MacroAction {
public:
	MacroActionProjector(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionProjector>(m);
	}

	enum class Type {
		SOURCE,
		SCENE,
		PREVIEW,
		PROGRAM,
		MULTIVIEW,
	};

	Type _type = Type::SCENE;
	SourceSelection _source;
	SceneSelection _scene;
	int _monitor = 0;
	bool _fullscreen = true;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionProjectorEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionProjectorEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionProjector> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionProjectorEdit(
			parent, std::dynamic_pointer_cast<MacroActionProjector>(
					action));
	}

private slots:
	void WindowTypeChanged(int value);
	void TypeChanged(int value);
	void SceneChanged(const SceneSelection &);
	void SourceChanged(const SourceSelection &);
	void MonitorChanged(int value);

protected:
	QComboBox *_windowTypes;
	QComboBox *_types;
	SceneSelectionWidget *_scenes;
	SourceSelectionWidget *_sources;
	QHBoxLayout *_monitorSelection;
	QComboBox *_monitors;
	std::shared_ptr<MacroActionProjector> _entryData;

private:
	void SetWidgetVisibility();
	bool _loading = true;
};

} // namespace advss
