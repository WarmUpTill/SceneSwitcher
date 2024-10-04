#include "chat-message-pattern.hpp"

#include <log-helper.hpp>
#include <obs-module-helper.hpp>
#include <QComboBox>
#include <ui-helpers.hpp>

namespace advss {

const std::vector<ChatMessageProperty::PropertyInfo> ChatMessageProperty::_supportedProperties = {
	{"firstMessage",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.firstMessage",
	 true,
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 return message.properties.isFirstMessage ==
			std::get<bool>(property._value);
	 }},
	{"emoteOnly",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.emoteOnly",
	 true,
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 return message.properties.isUsingOnlyEmotes ==
			std::get<bool>(property._value);
	 }},
	{"mod", "AdvSceneSwitcher.condition.twitch.type.chat.properties.mod",
	 true,
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 return message.properties.isMod ==
			std::get<bool>(property._value);
	 }},
	{"subscriber",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.subscriber",
	 true,
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 return message.properties.isSubscriber ==
			std::get<bool>(property._value);
	 }},
	{"turbo",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.turbo", true,
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 return message.properties.isTurbo ==
			std::get<bool>(property._value);
	 }},
	{"vip", "AdvSceneSwitcher.condition.twitch.type.chat.properties.vip",
	 true,
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 return message.properties.isVIP ==
			std::get<bool>(property._value);
	 }},
	{"color",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.color",
	 std::string(),
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 auto value = std::get<StringVariable>(property._value);
		 return !property._regex.Enabled()
				? message.properties.color == std::string(value)
				: property._regex.Matches(
					  message.properties.color, value);
	 }},
	{"displayName",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.displayName",
	 std::string(),
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 auto value = std::get<StringVariable>(property._value);
		 return !property._regex.Enabled()
				? message.properties.displayName ==
					  std::string(value)
				: property._regex.Matches(
					  message.properties.displayName,
					  value);
	 }},
	{"loginName",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.loginName",
	 std::string(),
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 auto value = std::get<StringVariable>(property._value);
		 return !property._regex.Enabled()
				? message.source.nick == std::string(value)
				: property._regex.Matches(message.source.nick,
							  value);
	 }},
	{"badge",
	 "AdvSceneSwitcher.condition.twitch.type.chat.properties.badge",
	 std::string("broadcaster"),
	 [](const IRCMessage &message, const ChatMessageProperty &property) {
		 auto value = std::get<StringVariable>(property._value);
		 for (const auto &badge : message.properties.badges) {
			 if (!badge.enabled) {
				 continue;
			 }
			 const bool badgeNameMatches =
				 !property._regex.Enabled()
					 ? badge.name == std::string(value)
					 : property._regex.Matches(badge.name,
								   value);
			 if (badgeNameMatches) {
				 return true;
			 }
		 }
		 return false;
	 },
	 true},
};

PropertySelectionDialog::PropertySelectionDialog(QWidget *parent)
	: QDialog(parent),
	  _selection(new QComboBox())
{
	setModal(true);
	setWindowModality(Qt::WindowModality::ApplicationModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setMinimumWidth(350);
	setMinimumHeight(70);

	_selection->setPlaceholderText(
		obs_module_text("AdvSceneSwitcher.item.select"));

	for (const auto &prop : ChatMessageProperty::GetSupportedIds()) {
		_selection->addItem(
			ChatMessageProperty::GetLocale(prop.c_str()),
			prop.c_str());
	}

	auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok |
					      QDialogButtonBox::Cancel);
	buttonbox->button(QDialogButtonBox::Ok)->setDisabled(true);

	// Cast is required to support all versions of Qt 5
	connect(_selection,
		static_cast<void (QComboBox::*)(int)>(
			&QComboBox::currentIndexChanged),
		this, [buttonbox](int idx) {
			buttonbox->button(QDialogButtonBox::Ok)
				->setDisabled(idx == -1);
		});

	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto layout = new QVBoxLayout();
	layout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.twitch.type.chat.properties.select")));
	layout->addWidget(_selection);
	layout->addWidget(buttonbox);
	setLayout(layout);
}

std::optional<ChatMessageProperty> PropertySelectionDialog::AskForPorperty(
	const std::vector<ChatMessageProperty> &currentProperties)
{
	PropertySelectionDialog dialog(GetSettingsWindow());
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));

	for (const auto &currentProperty : currentProperties) {
		if (currentProperty.IsReusable()) {
			continue;
		}

		const int idx = dialog._selection->findText(
			currentProperty.GetLocale());
		if (idx == -1) {
			continue;
		}

		qobject_cast<QListView *>(dialog._selection->view())
			->setRowHidden(idx, true);
	}

	if (dialog.exec() != DialogCode::Accepted) {
		return {};
	}

	const auto currentItem =
		dialog._selection->itemData(dialog._selection->currentIndex());
	const auto id = currentItem.toString().toStdString();
	return ChatMessageProperty{
		id, ChatMessageProperty::GetDefaultValue(id.c_str())};
}

ChatMessageEdit::ChatMessageEdit(QWidget *parent)
	: ListEditor(parent, false),
	  _message(new VariableTextEdit(this, 5, 1, 1)),
	  _regex(new RegexConfigWidget(parent))
{
	_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_list->setAutoScroll(false);

	connect(_message, &VariableTextEdit::textChanged, this,
		&ChatMessageEdit::MessageChanged);
	connect(_regex, &RegexConfigWidget::RegexConfigChanged, this,
		&ChatMessageEdit::RegexChanged);

	auto messageLayout = new QHBoxLayout();
	messageLayout->addWidget(_message);
	messageLayout->addWidget(_regex);

	_mainLayout->insertLayout(0, messageLayout);
	_mainLayout->insertWidget(
		1,
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.condition.twitch.type.chat.properties")));

	adjustSize();
	updateGeometry();
}

void ChatMessageEdit::SetMessagePattern(const ChatMessagePattern &pattern)
{
	_currentSelection = pattern;
	_message->setPlainText(pattern._message);
	_regex->SetRegexConfig(pattern._regex);

	_list->clear();
	_currentSelection._properties.clear();
	for (const auto &property : pattern._properties) {
		InsertElement(property);
	}
}

void ChatMessageEdit::Remove()
{
	auto item = _list->currentItem();
	int idx = _list->currentRow();
	if (!item || idx == -1) {
		return;
	}
	delete item;
	_currentSelection._properties.erase(
		_currentSelection._properties.begin() + idx);
	emit ChatMessagePatternChanged(_currentSelection);
	UpdateListSize();
}

void ChatMessageEdit::PropertyChanged(const ChatMessageProperty &property)
{
	int idx = GetIndexOfSignal();
	if (idx == -1) {
		return;
	}

	_currentSelection._properties.at(idx) = property;
	_list->setCurrentRow(idx);
	emit ChatMessagePatternChanged(_currentSelection);
}

void ChatMessageEdit::ElementFocussed()
{
	int idx = GetIndexOfSignal();
	if (idx == -1) {
		return;
	}
	_list->setCurrentRow(idx);
}

void ChatMessageEdit::MessageChanged()
{
	_currentSelection._message = _message->toPlainText().toStdString();
	emit ChatMessagePatternChanged(_currentSelection);
}

void ChatMessageEdit::RegexChanged(const RegexConfig &regex)
{
	_currentSelection._regex = regex;
	emit ChatMessagePatternChanged(_currentSelection);
	adjustSize();
	updateGeometry();
}

void ChatMessageEdit::InsertElement(const ChatMessageProperty &property)
{
	auto item = new QListWidgetItem(_list);
	_list->addItem(item);
	auto propertyEdit = new ChatMessagePropertyEdit(this, property);
	item->setSizeHint(propertyEdit->minimumSizeHint());
	_list->setItemWidget(item, propertyEdit);
	QWidget::connect(propertyEdit,
			 SIGNAL(PropertyChanged(const ChatMessageProperty &)),
			 this,
			 SLOT(PropertyChanged(const ChatMessageProperty &)));
	QWidget::connect(propertyEdit, SIGNAL(Focussed()), this,
			 SLOT(ElementFocussed()));
	_currentSelection._properties.push_back(property);
}

void ChatMessageEdit::Add()
{
	auto newProp = PropertySelectionDialog::AskForPorperty(
		_currentSelection._properties);
	if (!newProp) {
		return;
	}

	InsertElement(*newProp);
	emit ChatMessagePatternChanged(_currentSelection);
	UpdateListSize();
}

ChatMessagePropertyEdit::ChatMessagePropertyEdit(
	QWidget *parent, const ChatMessageProperty &property)
	: QWidget(parent),
	  _boolValue(new QCheckBox(property.GetLocale(), this)),
	  _textValue(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(this)),
	  _property(property)
{
	installEventFilter(this);

	if (std::holds_alternative<bool>(property._value)) {
		const bool value = std::get<bool>(property._value);
		_boolValue->setChecked(value);
	} else if (std::holds_alternative<StringVariable>(property._value)) {
		const auto value = std::get<StringVariable>(property._value);
		_textValue->setText(value);
		_regex->SetRegexConfig(property._regex);
	}

	connect(_boolValue, &QCheckBox::stateChanged, this, [this](int value) {
		_property._value = static_cast<bool>(value);
		emit PropertyChanged(_property);
	});
	connect(_textValue, &VariableLineEdit::editingFinished, this, [this]() {
		_property._value = _textValue->text().toStdString();
		emit PropertyChanged(_property);
	});
	connect(_regex, &RegexConfigWidget::RegexConfigChanged, this,
		[this](const RegexConfig &regex) {
			_property._regex = regex;
			emit PropertyChanged(_property);
			adjustSize();
			updateGeometry();
		});

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_boolValue);
	layout->addWidget(_textValue);
	if (std::holds_alternative<StringVariable>(property._value)) {
		layout->insertWidget(0, new QLabel(property.GetLocale()));
		layout->addWidget(_regex);
	}
	setLayout(layout);

	SetVisibility();
}

bool ChatMessagePropertyEdit::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress ||
	    event->type() == QEvent::MouseButtonDblClick) {
		emit Focussed();
	}

	return QWidget::eventFilter(obj, event);
}

void ChatMessagePropertyEdit::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	QWidgetList childWidgets = findChildren<QWidget *>();
	for (QWidget *childWidget : childWidgets) {
		childWidget->installEventFilter(this);
	}
}

void ChatMessagePropertyEdit::SetVisibility()
{
	auto defaultValue = _property.GetDefaultValue();
	_boolValue->setVisible(std::holds_alternative<bool>(defaultValue));
	const bool isTextValue =
		std::holds_alternative<StringVariable>(defaultValue);
	_textValue->setVisible(isTextValue);
	_regex->setVisible(isTextValue);
}

void ChatMessageProperty::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "id", _id.c_str());
	std::visit(
		[obj, this](auto &&arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, bool>) {
				obs_data_set_bool(obj, "boolValue", arg);
			} else if constexpr (std::is_same_v<T, StringVariable>) {
				arg.Save(obj, "strValue");
				_regex.Save(obj);
			} else {
				blog(LOG_WARNING,
				     "cannot save unknown chat message property");
			}
		},
		_value);
}

void ChatMessageProperty::Load(obs_data_t *obj)
{
	_id = obs_data_get_string(obj, "id");

	if (obs_data_has_user_value(obj, "strValue")) {
		StringVariable value;
		value.Load(obj, "strValue");
		_value = value;
		_regex.Load(obj);
	} else if (obs_data_has_user_value(obj, "boolValue")) {
		bool value = obs_data_get_bool(obj, "boolValue");
		_value = value;
	} else {
		blog(LOG_WARNING, "cannot load unknown chat message property");
	}
}

QString ChatMessageProperty::GetLocale(const char *id)
{
	auto it = std::find_if(_supportedProperties.begin(),
			       _supportedProperties.end(),
			       [id](const PropertyInfo &pi) {
				       return std::string(pi._id) == id;
			       });
	if (it == _supportedProperties.end()) {
		return "";
	}
	return obs_module_text(it->_locale);
}

std::variant<bool, StringVariable>
ChatMessageProperty::GetDefaultValue(const char *id)
{
	auto it = std::find_if(_supportedProperties.begin(),
			       _supportedProperties.end(),
			       [id](const PropertyInfo &pi) {
				       return std::string(pi._id) == id;
			       });
	if (it == _supportedProperties.end()) {
		return "";
	}
	return it->_defaultValue;
}

std::variant<bool, StringVariable> ChatMessageProperty::GetDefaultValue() const
{
	return GetDefaultValue(_id.c_str());
}

const std::vector<std::string> &ChatMessageProperty::GetSupportedIds()
{
	static std::vector<std::string> supportedIds;
	static bool setupDone = false;
	if (!setupDone) {
		for (const auto &prop : _supportedProperties) {
			supportedIds.emplace_back(prop._id);
		}
		setupDone = true;
	}
	return supportedIds;
}

bool ChatMessageProperty::Matches(const IRCMessage &message) const
{
	auto it = std::find_if(_supportedProperties.begin(),
			       _supportedProperties.end(),
			       [this](const PropertyInfo &pi) {
				       return std::string(pi._id) == _id;
			       });
	if (it == _supportedProperties.end()) {
		return false;
	}

	return it->_checkForMatch(message, *this);
}

bool ChatMessageProperty::IsReusable() const
{
	auto it = std::find_if(_supportedProperties.begin(),
			       _supportedProperties.end(),
			       [this](const PropertyInfo &pi) {
				       return std::string(pi._id) == _id;
			       });
	if (it == _supportedProperties.end()) {
		return false;
	}

	return it->_isReusable;
}

void ChatMessagePattern::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	_message.Save(data, "message");
	_regex.Save(data, "regex");

	OBSDataArrayAutoRelease properties = obs_data_array_create();
	for (const auto &property : _properties) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		property.Save(arrayObj);
		obs_data_array_push_back(properties, arrayObj);
	}
	obs_data_set_array(data, "properties", properties);
	obs_data_set_obj(obj, "chatMessagePattern", data);
}

void ChatMessagePattern::Load(obs_data_t *obj)
{
	if (!obs_data_has_user_value(obj, "chatMessagePattern")) {
		// Backward compatibility
		_message.Load(obj, "chatMessage");
		_regex.Load(obj, "regexChat");
		return;
	}

	OBSDataAutoRelease data = obs_data_get_obj(obj, "chatMessagePattern");
	_message.Load(data, "message");
	_regex.Load(data, "regex");

	OBSDataArrayAutoRelease properties =
		obs_data_get_array(data, "properties");
	size_t count = obs_data_array_count(properties);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj =
			obs_data_array_item(properties, i);
		ChatMessageProperty property;
		property.Load(arrayObj);
		_properties.push_back(property);
	}
}

bool ChatMessagePattern::Matches(const IRCMessage &chatMessage) const
{
	const bool messageMatch =
		!_regex.Enabled()
			? chatMessage.message == std::string(_message)
			: _regex.Matches(chatMessage.message, _message);
	if (!messageMatch) {
		return false;
	}

	for (const auto &property : _properties) {
		if (!property.Matches(chatMessage)) {
			return false;
		}
	}

	return true;
}

} // namespace advss
