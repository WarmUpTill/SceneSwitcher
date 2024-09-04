#pragma once
#include "macro-condition-edit.hpp"
#include "message-buffer.hpp"
#include "message-dispatcher.hpp"
#include "regex-config.hpp"
#include "variable-spinbox.hpp"
#include "variable-text-edit.hpp"

#include <QPushButton>

namespace advss {

struct StreamDeckMessage {
	bool keyDown = true;
	int row = 0;
	int column = 0;
	std::string data = "";
};

using StreamDeckMessageBuffer =
	std::shared_ptr<MessageBuffer<StreamDeckMessage>>;
using StreamDeckMessageDispatcher = MessageDispatcher<StreamDeckMessage>;
[[nodiscard]] StreamDeckMessageBuffer RegisterForStreamDeckMessages();

class MacroConditionStreamdeck : public MacroCondition {
public:
	MacroConditionStreamdeck(Macro *m);
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionStreamdeck>(m);
	}

	struct StreamDeckMessagePattern {
		void Save(obs_data_t *) const;
		void Load(obs_data_t *);

		bool checkKeyState = true;
		bool keyDown = true;
		bool checkPosition = true;
		IntVariable row;
		IntVariable column;
		bool checkData = true;
		StringVariable data;
		RegexConfig regex;
	};
	StreamDeckMessagePattern _pattern;

private:
	void SetTempVarValues(const StreamDeckMessage &);
	void SetupTempVars();
	bool MessageMatches(const StreamDeckMessage &);

	// Keep track of the key state so that the condition can still evaluate
	// to true while the key is held down.
	bool _lastMatchingKeyIsStillPressed = false;

	StreamDeckMessageBuffer _messageBuffer;

	static bool _registered;
	static const std::string id;
};

class MacroConditionStreamdeckEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionStreamdeckEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionStreamdeck> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionStreamdeckEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionStreamdeck>(
				cond));
	}

private slots:
	void CheckKeyStateChanged(int);
	void KeyStateChanged(int);
	void CheckPositionChanged(int);
	void RowChanged(const IntVariable &);
	void ColumnChanged(const IntVariable &);
	void CheckDataChanged(int);
	void RegexChanged(const RegexConfig &);
	void DataChanged();
	void ListenClicked();

private:
	void SetWidgetVisibility();

	QCheckBox *_checkKeyState;
	QComboBox *_keyState;
	QCheckBox *_checkPosition;
	VariableSpinBox *_row;
	VariableSpinBox *_column;
	QCheckBox *_checkData;
	VariableTextEdit *_data;
	RegexConfigWidget *_regex;
	QPushButton *_listen;

	bool _isListening = false;
	StreamDeckMessageBuffer _messageBuffer;
	QTimer _updateListenSettings;

	std::shared_ptr<MacroConditionStreamdeck> _entryData;
	bool _loading = true;
};

} // namespace advss
