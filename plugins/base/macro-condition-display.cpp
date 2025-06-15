#include "macro-condition-display.hpp"
#include "layout-helpers.hpp"

#include <QGuiApplication>
#include <QScreen>

namespace advss {

const std::string MacroConditionDisplay::id = "display";

bool MacroConditionDisplay::_registered = MacroConditionFactory::Register(
	MacroConditionDisplay::id,
	{MacroConditionDisplay::Create, MacroConditionDisplayEdit::Create,
	 "AdvSceneSwitcher.condition.display"});

static const std::map<MacroConditionDisplay::Condition, std::string>
	conditionTypes = {
		{MacroConditionDisplay::Condition::DISPLAY_NAME,
		 "AdvSceneSwitcher.condition.display.type.displayName"},
		{MacroConditionDisplay::Condition::DISPLAY_COUNT,
		 "AdvSceneSwitcher.condition.display.type.displayCount"},
		{MacroConditionDisplay::Condition::DISPLAY_WIDTH,
		 "AdvSceneSwitcher.condition.display.type.displayWidth"},
		{MacroConditionDisplay::Condition::DISPLAY_HEIGHT,
		 "AdvSceneSwitcher.condition.display.type.displayHeight"},
};

static const std::map<MacroConditionDisplay::CompareMode, std::string>
	compareModes = {
		{MacroConditionDisplay::CompareMode::EQUALS,
		 "AdvSceneSwitcher.condition.display.compare.equals"},
		{MacroConditionDisplay::CompareMode::LESS,
		 "AdvSceneSwitcher.condition.display.compare.less"},
		{MacroConditionDisplay::CompareMode::MORE,
		 "AdvSceneSwitcher.condition.display.compare.more"},
};

static bool compare(MacroConditionDisplay::CompareMode compare, int value1,
		    int value2)
{
	switch (compare) {
	case MacroConditionDisplay::CompareMode::EQUALS:
		return value1 == value2;
	case MacroConditionDisplay::CompareMode::LESS:
		return value1 < value2;
	case MacroConditionDisplay::CompareMode::MORE:
		return value1 > value2;
	default:
		break;
	}
	return false;
}

static std::optional<int> getMonitorSize(const QString &monitorName,
					 const RegexConfig &regex, bool height,
					 bool useDevicePixelRatio)
{
	auto monitorNames = GetMonitorNames();
	int index = -1;
	if (regex.Enabled()) {
		index = monitorNames.indexOf(
			regex.GetRegularExpression(monitorName));
	} else {
		index = monitorNames.indexOf(monitorName);
	}

	if (index == -1) {
		return {};
	}

	QList<QScreen *> screens = QGuiApplication::screens();
	if (index >= screens.size()) {
		return {};
	}

	auto screen = screens.at(index);
	if (!screen) {
		return {};
	}

	auto screenGeometry = screen->geometry();
	qreal ratio = useDevicePixelRatio ? screen->devicePixelRatio() : 1;

	return height ? screenGeometry.height() * ratio
		      : screenGeometry.width() * ratio;
}

static bool
matchDisplayNames(const std::string &name, const RegexConfig &regex,
		  std::function<void(const std::string &, const std::string &)>
			  setVariable)
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
				setVariable("name", m.toStdString());
				return true;
			}
		}
	} else {
		for (const auto &m : monitors) {
			if (m.toStdString() == name) {
				setVariable("name", m.toStdString());
				return true;
			}
		}
	}
	return false;
}

bool MacroConditionDisplay::CheckCondition()
{
	switch (_condition) {
	case Condition::DISPLAY_NAME:
		return matchDisplayNames(_displayName, _regexConf,
					 [this](const std::string &var,
						const std::string &value) {
						 SetTempVarValue(var, value);
					 });
	case Condition::DISPLAY_COUNT: {
		int count = QGuiApplication::screens().count();
		SetTempVarValue("count", std::to_string(count));
		return compare(_compare, count, _displayCount);
	}
	case Condition::DISPLAY_WIDTH: {
		auto size = getMonitorSize(QString::fromStdString(_displayName),
					   _regexConf, false,
					   _useDevicePixelRatio);
		if (size.has_value() &&
		    compare(_compare, *size, _displayWidth)) {
			SetTempVarValue("width", std::to_string(*size));
			return true;
		}
		return false;
	}
	case Condition::DISPLAY_HEIGHT: {
		auto size = getMonitorSize(QString::fromStdString(_displayName),
					   _regexConf, true,
					   _useDevicePixelRatio);
		if (size.has_value() &&
		    compare(_compare, *size, _displayHeight)) {
			SetTempVarValue("height", std::to_string(*size));
			return true;
		}
		return false;
	}
	default:
		break;
	}
	return false;
}

bool MacroConditionDisplay::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "compareMode", static_cast<int>(_compare));
	_displayName.Save(obj, "displayName");
	_regexConf.Save(obj);
	_displayCount.Save(obj, "displayCount");
	_displayWidth.Save(obj, "displayWidth");
	_displayHeight.Save(obj, "displayHeight");
	obs_data_set_bool(obj, "useDevicePixelRatio", _useDevicePixelRatio);
	return true;
}

bool MacroConditionDisplay::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	_compare =
		static_cast<CompareMode>(obs_data_get_int(obj, "compareMode"));
	_displayName.Load(obj, "displayName");
	_regexConf.Load(obj);
	_displayCount.Load(obj, "displayCount");
	_displayWidth.Load(obj, "displayWidth");
	_displayHeight.Load(obj, "displayHeight");
	obs_data_set_default_bool(obj, "useDevicePixelRatio", true);
	_useDevicePixelRatio = obs_data_get_bool(obj, "useDevicePixelRatio");
	return true;
}

void MacroConditionDisplay::SetCondition(Condition condition)
{
	_condition = condition;
	SetupTempVars();
}

void MacroConditionDisplay::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_condition) {
	case Condition::DISPLAY_NAME:
		AddTempvar(
			"name",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.name"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.name.description"));
		break;
	case Condition::DISPLAY_COUNT:
		AddTempvar(
			"count",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.count"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.count.description"));
		break;
	case Condition::DISPLAY_WIDTH:
		AddTempvar(
			"width",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.width"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.width.description"));
		break;
	case Condition::DISPLAY_HEIGHT:
		AddTempvar(
			"height",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.height"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.display.height.description"));
		break;
	default:
		break;
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static inline void populateCompareModeselection(QComboBox *list)
{
	for (const auto &[_, name] : compareModes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionDisplayEdit::MacroConditionDisplayEdit(
	QWidget *parent, std::shared_ptr<MacroConditionDisplay> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _compareModes(new QComboBox()),
	  _displays(new MonitorSelectionWidget(this)),
	  _regex(new RegexConfigWidget()),
	  _displayCount(new VariableSpinBox(this)),
	  _displayWidth(new VariableSpinBox(this)),
	  _displayHeight(new VariableSpinBox(this)),
	  _useDevicePixelRatio(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.display.useDevicePixelRatio")))
{
	populateConditionSelection(_conditions);
	populateCompareModeselection(_compareModes);
	_displayWidth->setMaximum(99999);
	_displayHeight->setMaximum(99999);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_compareModes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CompareModeChanged(int)));
	QWidget::connect(_displays, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(DisplayNameChanged(const QString &)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(
		_displayCount,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(DisplayCountChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_displayWidth,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(DisplayWidthChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_displayHeight,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(DisplayHeightChanged(const NumberVariable<int> &)));
	QWidget::connect(_useDevicePixelRatio, SIGNAL(stateChanged(int)), this,
			 SLOT(UseDevicePixelRatioChanged(int)));

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.display.entry"),
		entryLayout,
		{{"{{conditions}}", _conditions},
		 {"{{compareModes}}", _compareModes},
		 {"{{displays}}", _displays},
		 {"{{regex}}", _regex},
		 {"{{displayCount}}", _displayCount},
		 {"{{displayWidth}}", _displayWidth},
		 {"{{displayHeight}}", _displayHeight}});
	auto layout = new QVBoxLayout;
	layout->addLayout(entryLayout);
	layout->addWidget(_useDevicePixelRatio);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionDisplayEdit::ConditionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCondition(
		static_cast<MacroConditionDisplay::Condition>(value));
	SetWidgetVisibility();
}

void MacroConditionDisplayEdit::CompareModeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_compare =
		static_cast<MacroConditionDisplay::CompareMode>(value);
}

void MacroConditionDisplayEdit::DisplayNameChanged(const QString &display)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_displayName = display.toStdString();
}

void MacroConditionDisplayEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regexConf = conf;

	adjustSize();
	updateGeometry();
}

void MacroConditionDisplayEdit::DisplayCountChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_displayCount = value;
}

void MacroConditionDisplayEdit::DisplayWidthChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_displayWidth = value;
}

void MacroConditionDisplayEdit::DisplayHeightChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_displayHeight = value;
}

void MacroConditionDisplayEdit::UseDevicePixelRatioChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useDevicePixelRatio = state;
}

void MacroConditionDisplayEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_compareModes->setCurrentIndex(static_cast<int>(_entryData->_compare));
	_displays->setCurrentText(QString::fromStdString(
		_entryData->_displayName.UnresolvedValue()));
	_regex->SetRegexConfig(_entryData->_regexConf);
	_displayCount->SetValue(_entryData->_displayCount);
	_displayWidth->SetValue(_entryData->_displayWidth);
	_displayHeight->SetValue(_entryData->_displayHeight);
	_useDevicePixelRatio->setChecked(_entryData->_useDevicePixelRatio);
	SetWidgetVisibility();
}

void MacroConditionDisplayEdit::SetWidgetVisibility()
{
	_displays->setVisible(
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_NAME ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_WIDTH ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_HEIGHT);
	_compareModes->setVisible(
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_WIDTH ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_HEIGHT ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_COUNT);
	_regex->setVisible(
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_NAME ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_WIDTH ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_HEIGHT);
	_displayCount->setVisible(
		_entryData->GetCondition() ==
		MacroConditionDisplay::Condition::DISPLAY_COUNT);
	_displayWidth->setVisible(
		_entryData->GetCondition() ==
		MacroConditionDisplay::Condition::DISPLAY_WIDTH);
	_displayHeight->setVisible(
		_entryData->GetCondition() ==
		MacroConditionDisplay::Condition::DISPLAY_HEIGHT);
	_useDevicePixelRatio->setVisible(
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_WIDTH ||
		_entryData->GetCondition() ==
			MacroConditionDisplay::Condition::DISPLAY_HEIGHT);
	adjustSize();
	updateGeometry();
}

} // namespace advss
