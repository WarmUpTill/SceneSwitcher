#include "macro-condition-display.hpp"
#include "utility.hpp"

#include <QGuiApplication>

namespace advss {

const std::string MacroConditionDisplay::id = "display";

bool MacroConditionDisplay::_registered = MacroConditionFactory::Register(
	MacroConditionDisplay::id,
	{MacroConditionDisplay::Create, MacroConditionDisplayEdit::Create,
	 "AdvSceneSwitcher.condition.display"});

const static std::map<MacroConditionDisplay::Condition, std::string>
	conditionTypes = {
		{MacroConditionDisplay::Condition::DISPLAY_NAME,
		 "AdvSceneSwitcher.condition.display.type.displayName"},
		{MacroConditionDisplay::Condition::DISPLAY_COUNT,
		 "AdvSceneSwitcher.condition.display.type.displayCount"},
};

static bool matchDisplayNames(const std::string &name, const RegexConfig &regex)
{
	auto monitors = GetMonitorNames();
	if (regex.Enabled()) {
		auto expr = regex.GetRegularExpression(name);
		if (!expr.isValid()) {
			return false;
		}

		for (const auto &m : monitors) {
			auto match = expr.match(m);
			if (match.hasMatch()) {
				return true;
			}
		}
	} else {
		for (const auto &m : monitors) {
			if (m.toStdString() == name) {
				return true;
			}
		}
	}
	return false;
}

bool MacroConditionDisplay::CheckCondition()
{
	switch (_condition) {
	case advss::MacroConditionDisplay::Condition::DISPLAY_NAME:
		return matchDisplayNames(_displayName, _regexConf);
	case advss::MacroConditionDisplay::Condition::DISPLAY_COUNT:
		return _displayCount == QGuiApplication::screens().count();
	default:
		break;
	}
	return false;
}

bool MacroConditionDisplay::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	_displayName.Save(obj, "displayName");
	_regexConf.Save(obj);
	_displayCount.Save(obj, "displayCount");
	return true;
}

bool MacroConditionDisplay::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_displayName.Load(obj, "displayName");
	_regexConf.Load(obj);
	_displayCount.Load(obj, "displayCount");
	return true;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionDisplayEdit::MacroConditionDisplayEdit(
	QWidget *parent, std::shared_ptr<MacroConditionDisplay> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _displays(new QComboBox()),
	  _regex(new RegexConfigWidget()),
	  _displayCount(new VariableSpinBox(this))
{
	populateConditionSelection(_conditions);
	_displays->addItems(GetMonitorNames());
	_displays->setEditable(true);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_displays, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(DisplayNameChanged(const QString &)));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));
	QWidget::connect(
		_displayCount,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(DisplayCountChanged(const NumberVariable<int> &)));

	auto mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{conditions}}", _conditions},
		{"{{displays}}", _displays},
		{"{{regex}}", _regex},
		{"{{displayCount}}", _displayCount},
	};
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.display.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionDisplayEdit::ConditionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionDisplay::Condition>(value);
	SetWidgetVisibility();
}

void MacroConditionDisplayEdit::DisplayNameChanged(const QString &display)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_displayName = display.toStdString();
}

void MacroConditionDisplayEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regexConf = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionDisplayEdit::DisplayCountChanged(
	const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_displayCount = value;
}

void MacroConditionDisplayEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_displays->setCurrentText(QString::fromStdString(
		_entryData->_displayName.UnresolvedValue()));
	_regex->SetRegexConfig(_entryData->_regexConf);
	_displayCount->SetValue(_entryData->_displayCount);
	SetWidgetVisibility();
}

void MacroConditionDisplayEdit::SetWidgetVisibility()
{
	_displays->setVisible(_entryData->_condition ==
			      MacroConditionDisplay::Condition::DISPLAY_NAME);
	_regex->setVisible(_entryData->_condition ==
			   MacroConditionDisplay::Condition::DISPLAY_NAME);
	_displayCount->setVisible(
		_entryData->_condition ==
		MacroConditionDisplay::Condition::DISPLAY_COUNT);
}

} // namespace advss
