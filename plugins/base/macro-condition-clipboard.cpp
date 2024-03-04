#include "macro-condition-clipboard.hpp"
#include "layout-helpers.hpp"
#include "ui-helpers.hpp"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>

namespace advss {

const std::string MacroConditionClipboard::id = "clipboard";

bool MacroConditionClipboard::_registered = MacroConditionFactory::Register(
	MacroConditionClipboard::id,
	{MacroConditionClipboard::Create, MacroConditionClipboardEdit::Create,
	 "AdvSceneSwitcher.condition.clipboard"});

const static std::map<MacroConditionClipboard::Condition, std::string>
	conditionTypes = {
		{MacroConditionClipboard::Condition::CHANGED,
		 "AdvSceneSwitcher.condition.clipboard.condition.changed"},
		{MacroConditionClipboard::Condition::IS_TEXT,
		 "AdvSceneSwitcher.condition.clipboard.condition.isText"},
		{MacroConditionClipboard::Condition::IS_IMAGE,
		 "AdvSceneSwitcher.condition.clipboard.condition.isImage"},
		{MacroConditionClipboard::Condition::IS_URL,
		 "AdvSceneSwitcher.condition.clipboard.condition.isURL"},
		{MacroConditionClipboard::Condition::MATCHES,
		 "AdvSceneSwitcher.condition.clipboard.condition.matches"},
};

ClipboardMessageBuffer ClipboardListener::RegisterForClipboardChanges()
{
	return Instance()->_dispatcher.RegisterClient();
}

ClipboardListener *ClipboardListener::Instance()
{
	static ClipboardListener instance;
	return &instance;
}

ClipboardListener::ClipboardListener() : QObject(nullptr)
{
	if (!QApplication::clipboard()) {
		return;
	}
	QObject::connect(QApplication::clipboard(), &QClipboard::dataChanged,
			 this, &ClipboardListener::ClipboardDataChanged);
}

void ClipboardListener::ClipboardDataChanged()
{
	auto text = QApplication::clipboard()->text();
	_dispatcher.DispatchMessage(text.toStdString());
}

MacroConditionClipboard::MacroConditionClipboard(Macro *m) : MacroCondition(m)
{
	_messageBuffer = ClipboardListener::RegisterForClipboardChanges();
}

std::shared_ptr<MacroCondition> MacroConditionClipboard::Create(Macro *m)
{
	return std::make_shared<MacroConditionClipboard>(m);
}

static bool clipboardContainsImage()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();
	return mimeData->hasImage();
}

static bool clipboardContainsText()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();
	return mimeData->hasText();
}

static bool clipboardContainsUrl()
{
	const QClipboard *clipboard = QApplication::clipboard();
	const QMimeData *mimeData = clipboard->mimeData();
	return mimeData->hasUrls();
}

static std::string getCurrentClipboardText()
{
	auto clipboard = QApplication::clipboard();
	if (!clipboard) {
		return "";
	}
	return clipboard->text().toStdString();
}

bool MacroConditionClipboard::CheckCondition()
{
	switch (_condition) {
	case Condition::CHANGED:
		while (!_messageBuffer->Empty()) {
			auto message = _messageBuffer->ConsumeMessage();
			if (!message) {
				continue;
			}
			SetTempVarValue("text", *message);
			return true;
		}
		return false;
	case Condition::IS_TEXT:
		if (clipboardContainsText()) {
			SetTempVarValue("text", getCurrentClipboardText());
			return true;
		}
		return false;
	case Condition::IS_IMAGE:
		return clipboardContainsImage();
	case Condition::IS_URL:
		if (clipboardContainsUrl()) {
			SetTempVarValue("text", getCurrentClipboardText());
			return true;
		}
		return false;
	case Condition::MATCHES:
		if (_regex.Enabled()) {
			auto text = getCurrentClipboardText();
			if (_regex.Matches(text, _text)) {
				SetTempVarValue("text", text);
				return true;
			}
		} else {
			auto text = getCurrentClipboardText();
			if (text == std::string(_text)) {
				SetTempVarValue("text", text);
				return true;
			}
		}
		return false;
	default:
		break;
	}

	return false;
}

void MacroConditionClipboard::SetCondition(Condition condition)
{
	_condition = condition;
	_messageBuffer.reset();
	if (_condition == Condition::CHANGED) {
		_messageBuffer =
			ClipboardListener::RegisterForClipboardChanges();
	}
}

void MacroConditionClipboard::SetupTempVars()
{
	MacroCondition::SetupTempVars();

	AddTempvar(
		"text",
		obs_module_text("AdvSceneSwitcher.tempVar.clipboard.text"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.clipboard.text.description"));
}

bool MacroConditionClipboard::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_text.Save(obj, "text");
	_regex.Save(obj);
	return true;
}

bool MacroConditionClipboard::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_text.Load(obj, "text");
	_regex.Load(obj);
	SetCondition(
		static_cast<Condition>(obs_data_get_int(obj, "condition")));
	return true;
}

static void populateCompareModeselection(QComboBox *list)
{
	for (const auto &[_, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionClipboardEdit::MacroConditionClipboardEdit(
	QWidget *parent, std::shared_ptr<MacroConditionClipboard> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox(this)),
	  _text(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(parent)),
	  _urlInfo(new QLabel())
{
	populateCompareModeselection(_conditions);

	QString path = GetThemeTypeName() == "Light"
			       ? ":/res/images/help.svg"
			       : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_urlInfo->setPixmap(pixmap);
	_urlInfo->hide();
	_urlInfo->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.clipboard.urlTooltip"));

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_text, SIGNAL(textChanged()), this,
			 SLOT(TextChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));

	auto layout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.clipboard.condition.entry"),
		layout,
		{{"{{conditions}}", _conditions},
		 {"{{regex}}", _regex},
		 {"{{urlInfo}}", _urlInfo}});

	auto mainLayout = new QVBoxLayout();
	mainLayout->addLayout(layout);
	mainLayout->addWidget(_text);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

QWidget *
MacroConditionClipboardEdit::Create(QWidget *parent,
				    std::shared_ptr<MacroCondition> action)
{
	return new MacroConditionClipboardEdit(
		parent,
		std::dynamic_pointer_cast<MacroConditionClipboard>(action));
}

void MacroConditionClipboardEdit::ConditionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetCondition(
		static_cast<MacroConditionClipboard::Condition>(value));
	SetWidgetVisibility();
}

void MacroConditionClipboardEdit::TextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_text = _text->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroConditionClipboardEdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionClipboardEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_text->setPlainText(_entryData->_text);
	_regex->SetRegexConfig(_entryData->_regex);
	SetWidgetVisibility();
}

void MacroConditionClipboardEdit::SetWidgetVisibility()
{
	const bool requiresTextInput =
		_entryData->GetCondition() ==
		MacroConditionClipboard::Condition::MATCHES;
	_regex->setVisible(requiresTextInput);
	_text->setVisible(requiresTextInput);
	_urlInfo->setVisible(_entryData->GetCondition() ==
			     MacroConditionClipboard::Condition::IS_URL);
	adjustSize();
	updateGeometry();
}

} // namespace advss
