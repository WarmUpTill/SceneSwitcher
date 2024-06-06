#pragma once
#include "macro-condition-edit.hpp"
#include "filter-combo-box.hpp"
#include "help-icon.hpp"
#include "message-buffer.hpp"
#include "message-dispatcher.hpp"
#include "regex-config.hpp"
#include "variable-text-edit.hpp"

namespace advss {

using ClipboardMessageBuffer = std::shared_ptr<MessageBuffer<std::string>>;
using ClipboardMessageDispatcher = MessageDispatcher<std::string>;

class ClipboardListener : public QObject {
	Q_OBJECT
public:
	[[nodiscard]] static ClipboardMessageBuffer
	RegisterForClipboardChanges();

private slots:
	void ClipboardDataChanged();

private:
	ClipboardListener();
	static ClipboardListener *Instance();

	ClipboardMessageDispatcher _dispatcher;
};

class MacroConditionClipboard : public MacroCondition {
public:
	MacroConditionClipboard(Macro *m);
	static std::shared_ptr<MacroCondition> Create(Macro *m);
	std::string GetId() const { return id; };
	bool CheckCondition();

	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	enum class Condition {
		CHANGED,
		IS_TEXT,
		IS_IMAGE,
		IS_URL,
		MATCHES,
	};
	void SetCondition(Condition);
	Condition GetCondition() const { return _condition; }

	StringVariable _text = obs_module_text(
		"AdvSceneSwitcher.condition.clipboard.placeholder");
	RegexConfig _regex;

private:
	void SetupTempVars();

	Condition _condition = Condition::CHANGED;
	ClipboardMessageBuffer _messageBuffer;

	static bool _registered;
	static const std::string id;
};

class MacroConditionClipboardEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionClipboardEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionClipboard> entryData = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> action);

private slots:
	void ConditionChanged(int);
	void TextChanged();
	void RegexChanged(const RegexConfig &conf);

private:
	void UpdateEntryData();
	void SetWidgetVisibility();

	QComboBox *_conditions;
	VariableTextEdit *_text;
	RegexConfigWidget *_regex;
	HelpIcon *_urlInfo;

	std::shared_ptr<MacroConditionClipboard> _entryData;
	bool _loading = true;
};

} // namespace advss
