#pragma once
#include "chat-connection.hpp"

#include <list-editor.hpp>
#include <regex-config.hpp>
#include <variable-text-edit.hpp>

namespace advss {

class ChatMessageProperty {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	std::string _id;
	std::variant<bool, StringVariable> _value;
	RegexConfig _regex;

	static QString GetLocale(const char *id);
	QString GetLocale() const { return GetLocale(_id.c_str()); }
	static std::variant<bool, StringVariable>
	GetDefaultValue(const char *id);
	std::variant<bool, StringVariable> GetDefaultValue() const;
	static const std::vector<std::string> &GetSupportedIds();
	bool Matches(const IRCMessage &) const;
	bool IsReusable() const;

private:
	struct PropertyInfo {
		const char *_id;
		const char *_locale;
		std::variant<bool, StringVariable> _defaultValue;
		std::function<bool(const IRCMessage &,
				   const ChatMessageProperty &)>
			_checkForMatch;
		bool _isReusable = false;
	};
	static const std::vector<PropertyInfo> _supportedProperties;
};

class ChatMessagePattern {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	bool Matches(const IRCMessage &) const;

	StringVariable _message = ".*";
	RegexConfig _regex = RegexConfig::PartialMatchRegexConfig(true);
	std::vector<ChatMessageProperty> _properties;
};

class PropertySelectionDialog : public QDialog {
	Q_OBJECT

public:
	PropertySelectionDialog(QWidget *parent);
	static std::optional<ChatMessageProperty> AskForPorperty(
		const std::vector<ChatMessageProperty> &currentProperties);

private:
	QComboBox *_selection;
};

class ChatMessagePropertyEdit : public QWidget {
	Q_OBJECT

public:
	ChatMessagePropertyEdit(QWidget *, const ChatMessageProperty &);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void showEvent(QShowEvent *event) override;

signals:
	void PropertyChanged(const ChatMessageProperty &);
	void Focussed();

private:
	void SetVisibility();

	QCheckBox *_boolValue;
	VariableLineEdit *_textValue;
	RegexConfigWidget *_regex;

	ChatMessageProperty _property;
};

class ChatMessageEdit final : public ListEditor {
	Q_OBJECT
public:
	ChatMessageEdit(QWidget *parent);
	void SetMessagePattern(const ChatMessagePattern &);

private slots:
	void MessageChanged();
	void RegexChanged(const RegexConfig &);
	void Add();
	void Remove();
	void PropertyChanged(const ChatMessageProperty &);
	void ElementFocussed();

signals:
	void ChatMessagePatternChanged(const ChatMessagePattern &);

private:
	void InsertElement(const ChatMessageProperty &);

	VariableTextEdit *_message;
	RegexConfigWidget *_regex;
	ChatMessagePattern _currentSelection;
};

} // namespace advss
