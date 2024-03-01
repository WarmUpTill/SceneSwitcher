#pragma once
#include "macro-action-edit.hpp"
#include "filter-combo-box.hpp"

#include <variable-line-edit.hpp>

namespace advss {

struct ClipboardQueueParams {
	std::string mimeTypePrimary;
	std::string mimeTypeAll;
};

struct ClipboardTextQueueParams : ClipboardQueueParams {
	StringVariable text;
};

struct ClipboardImageQueueParams : ClipboardQueueParams {
	StringVariable url;
};

class MacroActionClipboard : public MacroAction {
public:
	MacroActionClipboard(Macro *m) : MacroAction(m) {}
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	std::string GetId() const { return id; };

	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	void ResolveVariablesToFixedValues();

	enum class Action {
		COPY_TEXT,
		COPY_IMAGE,
	};

	Action _action = Action::COPY_TEXT;

	StringVariable _text = obs_module_text(
		"AdvSceneSwitcher.action.clipboard.copy.text.text.placeholder");
	StringVariable _url = obs_module_text(
		"AdvSceneSwitcher.action.clipboard.copy.image.url.placeholder");

private:
	void SetupTempVars();
	void SetTempVarValues(const ClipboardQueueParams &params);

	static bool _registered;
	static const std::string id;
};

class MacroActionClipboardEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionClipboardEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionClipboard> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionClipboardEdit(
			parent, std::dynamic_pointer_cast<MacroActionClipboard>(
					action));
	}
	void UpdateEntryData();

protected:
	std::shared_ptr<MacroActionClipboard> _entryData;

private slots:
	void ActionChanged(int index);
	void TextChanged();
	void UrlChanged();

private:
	void SetWidgetVisibility();

	bool _loading = true;

	FilterComboBox *_actions;
	VariableLineEdit *_text;
	VariableLineEdit *_url;
};

} // namespace advss
