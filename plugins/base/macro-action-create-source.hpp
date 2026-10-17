#pragma once
#include "filter-combo-box.hpp"
#include "macro-action-edit.hpp"
#include "scene-selection.hpp"
#include "variable-line-edit.hpp"
#include "variable-string.hpp"

namespace advss {

class MacroActionCreateSource : public MacroAction {
public:
	MacroActionCreateSource(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	SceneSelection _scene;
	std::string _sourceType;
	StringVariable _sourceName = obs_module_text(
		"AdvSceneSwitcher.action.createSource.name.default");

private:
	void SetupTempVars();

	static bool _registered;
	static const std::string id;
};

class MacroActionCreateSourceEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionCreateSourceEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionCreateSource> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionCreateSourceEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionCreateSource>(
				action));
	}

private slots:
	void SceneChanged(const SceneSelection &);
	void SourceTypeChanged(int);
	void SourceNameChanged();
signals:
	void HeaderInfoChanged(const QString &);

private:
	SceneSelectionWidget *_scenes;
	FilterComboBox *_sourceTypes;
	VariableLineEdit *_sourceName;
	std::shared_ptr<MacroActionCreateSource> _entryData;
	bool _loading = true;
};

} // namespace advss
