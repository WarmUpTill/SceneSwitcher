#include "temp-variable.hpp"
#include "advanced-scene-switcher.hpp"
#include "obs-module-helper.hpp"
#include "macro.hpp"
#include "macro-segment.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <QVariant>
#include <QAbstractItemView>

Q_DECLARE_METATYPE(advss::TempVariableRef);

namespace advss {

TempVariable::TempVariable(const std::string &id, const std::string &name,
			   const std::string &description,
			   const std::shared_ptr<MacroSegment> &segment)
	: _id(id),
	  _name(name),
	  _description(description),
	  _segment(segment)
{
}

TempVariable::TempVariable(const TempVariable &other) noexcept
{
	_id = other._id;
	_value = other._value;
	_name = other._name;
	_description = other._description;
	_valueIsValid = other._valueIsValid;
	_segment = other._segment;

	std::lock_guard<std::mutex> lock(other._lastValuesMutex);
	_lastValues = other._lastValues;
}

TempVariable::TempVariable(const TempVariable &&other) noexcept
{
	_id = other._id;
	_value = other._value;
	_name = other._name;
	_description = other._description;
	_valueIsValid = other._valueIsValid;
	_segment = other._segment;

	std::lock_guard<std::mutex> lock(other._lastValuesMutex);
	_lastValues = other._lastValues;
}

TempVariable &TempVariable::operator=(const TempVariable &other) noexcept
{
	if (this != &other) {
		_id = other._id;
		_value = other._value;
		_name = other._name;
		_description = other._description;
		_valueIsValid = other._valueIsValid;
		_segment = other._segment;

		std::lock_guard<std::mutex> lockOther(other._lastValuesMutex);
		std::lock_guard<std::mutex> lock(_lastValuesMutex);
		_lastValues = other._lastValues;
	}
	return *this;
}

TempVariable &TempVariable::operator=(const TempVariable &&other) noexcept
{
	if (this != &other) {
		_id = other._id;
		_value = other._value;
		_name = other._name;
		_description = other._description;
		_valueIsValid = other._valueIsValid;
		_segment = other._segment;

		std::lock_guard<std::mutex> lockOther(other._lastValuesMutex);
		std::lock_guard<std::mutex> lock(_lastValuesMutex);
		_lastValues = other._lastValues;
	}
	return *this;
}

std::optional<std::string> TempVariable::Value() const
{
	if (!_valueIsValid) {
		return {};
	}
	return _value;
}

void TempVariable::SetValue(const std::string &val)
{
	_valueIsValid = true;
	if (_value == val) {
		return;
	}
	_value = val;
	std::lock_guard<std::mutex> lock(_lastValuesMutex);
	if (_lastValues.size() >= 3) {
		_lastValues.erase(_lastValues.begin());
	}
	_lastValues.push_back(val);
}

void TempVariable::InvalidateValue()
{
	_valueIsValid = false;
}

TempVariableRef TempVariable::GetRef() const
{
	TempVariableRef ref;
	ref._id = _id;
	ref._segment = _segment;
	return ref;
}

TempVariableRef::SegmentType TempVariableRef::GetType() const
{
	auto segment = _segment.lock();
	if (!segment) {
		return SegmentType::NONE;
	}
	auto macro = segment->GetMacro();
	if (!macro) {
		return SegmentType::NONE;
	}

	if (std::find(macro->Conditions().begin(), macro->Conditions().end(),
		      segment) != macro->Conditions().end()) {
		return SegmentType::CONDITION;
	}
	if (std::find(macro->Actions().begin(), macro->Actions().end(),
		      segment) != macro->Actions().end()) {
		return SegmentType::ACTION;
	}
	if (std::find(macro->ElseActions().begin(), macro->ElseActions().end(),
		      segment) != macro->ElseActions().end()) {
		return SegmentType::ELSEACTION;
	}
	return SegmentType::NONE;
}

int TempVariableRef::GetIdx() const
{
	auto segment = _segment.lock();
	if (!segment) {
		return -1;
	}
	return segment->GetIndex();
}

void TempVariableRef::Save(obs_data_t *obj, const char *name) const
{
	OBSDataAutoRelease data = obs_data_create();
	auto type = GetType();
	obs_data_set_int(data, "type", static_cast<int>(type));
	obs_data_set_int(data, "idx", GetIdx());
	obs_data_set_string(data, "id", _id.c_str());
	obs_data_set_obj(obj, name, data);
}

void TempVariableRef::Load(obs_data_t *obj, Macro *macro, const char *name)
{
	if (!macro) {
		_segment.reset();
		return;
	}
	OBSDataAutoRelease data = obs_data_get_obj(obj, name);
	int idx = obs_data_get_int(data, "idx");
	_id = obs_data_get_string(data, "id");
	const auto type =
		static_cast<SegmentType>(obs_data_get_int(data, "type"));
	AddPostLoadStep([this, idx, type, macro]() {
		this->PostLoad(idx, type, macro);
	});
}

void TempVariableRef::PostLoad(int idx, SegmentType type, Macro *macro)
{
	if (!macro) {
		return;
	}

	std::deque<std::shared_ptr<MacroSegment>> segments;
	switch (type) {
	case SegmentType::NONE:
		_segment.reset();
		return;
	case SegmentType::CONDITION:
		segments = {macro->Conditions().begin(),
			    macro->Conditions().end()};
		break;
	case SegmentType::ACTION:
		segments = {macro->Actions().begin(), macro->Actions().end()};
		break;
	case SegmentType::ELSEACTION:
		segments = {macro->ElseActions().begin(),
			    macro->ElseActions().end()};
		break;
	default:
		break;
	}
	if (idx < 0 || idx >= (int)segments.size()) {
		_segment.reset();
		return;
	}
	_segment = segments[idx];
}

std::optional<const TempVariable>
TempVariableRef::GetTempVariable(Macro *macro) const
{
	if (!macro) {
		return {};
	}
	auto segment = _segment.lock();
	if (!segment) {
		return {};
	}

	return macro->GetTempVar(segment.get(), _id);
}

bool TempVariableRef::operator==(const TempVariableRef &other) const
{
	if (_id != other._id) {
		return false;
	}
	auto segment = _segment.lock();
	if (!segment) {
		return false;
	}
	return segment == other._segment.lock();
}

TempVariableSelection::TempVariableSelection(QWidget *parent)
	: QWidget(parent),
	  _selection(new FilterComboBox(
		  this, obs_module_text("AdvSceneSwitcher.tempVar.select"))),
	  _info(new AutoUpdateTooltipLabel(this, [this]() {
		  return SetupInfoLabel();
	  }))
{
	QString path = GetThemeTypeName() == "Light"
			       ? ":/res/images/help.svg"
			       : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_info->setPixmap(pixmap);
	_info->hide();

	_selection->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_selection->setMaximumWidth(350);
	_selection->setDuplicatesEnabled(true);
	PopulateSelection();

	QWidget::connect(_selection, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionIdxChanged(int)));
	QWidget::connect(_selection, SIGNAL(highlighted(int)), this,
			 SLOT(HighlightChanged(int)));
	QWidget::connect(window(), SIGNAL(MacroSegmentOrderChanged()), this,
			 SLOT(MacroSegmentsChanged()));
	QWidget::connect(window(), SIGNAL(SegmentTempVarsChanged()), this,
			 SLOT(SegmentTempVarsChanged()));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_selection);
	layout->addWidget(_info);
	setLayout(layout);
}

void TempVariableSelection::SelectionIdxChanged(int idx)
{
	if (idx == -1) {
		return;
	}
	auto var = _selection->itemData(idx).value<TempVariableRef>();
	emit SelectionChanged(var);
	HighlightSelection(var);
	SetupInfoLabel();
}

void TempVariableSelection::SetVariable(const TempVariableRef &var)
{
	const QSignalBlocker b(_selection);
	QVariant variant;
	variant.setValue(var);
	_selection->setCurrentIndex(_selection->findData(variant));
	SetupInfoLabel();
}

void TempVariableSelection::MacroSegmentsChanged()
{
	auto currentSelection = _selection->itemData(_selection->currentIndex())
					.value<TempVariableRef>();
	const QSignalBlocker b(_selection);
	_selection->clear();
	PopulateSelection();
	SetVariable(currentSelection);
}

void TempVariableSelection::SegmentTempVarsChanged()
{
	MacroSegmentsChanged();
}

void TempVariableSelection::HighlightChanged(int idx)
{
	auto var = _selection->itemData(idx).value<TempVariableRef>();
	HighlightSelection(var);
}

void TempVariableSelection::PopulateSelection()
{
	auto advssWindow = qobject_cast<AdvSceneSwitcher *>(window());
	if (!advssWindow) {
		return;
	}

	auto macro = advssWindow->GetSelectedMacro();
	if (!macro) {
		return;
	}

	auto vars = macro->GetTempVars(GetSegment());
	for (const auto &var : vars) {
		QVariant variant;
		variant.setValue(var.GetRef());
		_selection->addItem(QString::fromStdString(var.Name()),
				    variant);
	}

	// Make sure content is readable when expanding combobox
	auto view = _selection->view();
	if (view) {
		view->setMinimumWidth(view->sizeHintForColumn(0));
		_selection->setMinimumWidth(view->sizeHintForColumn(0));
		view->updateGeometry();
		_selection->updateGeometry();
	}

	adjustSize();
	updateGeometry();
}

void TempVariableSelection::HighlightSelection(const TempVariableRef &var)
{
	auto advssWindow = qobject_cast<AdvSceneSwitcher *>(window());
	if (!advssWindow) {
		return;
	}

	auto type = var.GetType();
	switch (type) {
	case TempVariableRef::SegmentType::NONE:
		return;
	case TempVariableRef::SegmentType::CONDITION:
		advssWindow->HighlightCondition(var.GetIdx(), Qt::white);
		return;
	case TempVariableRef::SegmentType::ACTION:
		advssWindow->HighlightAction(var.GetIdx(), Qt::white);
		return;
	case TempVariableRef::SegmentType::ELSEACTION:
		advssWindow->HighlightElseAction(var.GetIdx(), Qt::white);
		return;
	default:
		break;
	}
}

QString TempVariableSelection::SetupInfoLabel()
{
	auto currentSelection = _selection->itemData(_selection->currentIndex())
					.value<TempVariableRef>();
	auto advssWindow = qobject_cast<AdvSceneSwitcher *>(window());
	if (!advssWindow) {
		_info->setToolTip("");
		_info->hide();
		return "";
	}
	auto macro = advssWindow->GetSelectedMacro();
	if (!macro) {
		_info->setToolTip("");
		_info->hide();
		return "";
	}
	auto var = currentSelection.GetTempVariable(macro.get());
	if (!var) {
		_info->setToolTip("");
		_info->hide();
		return "";
	}
	auto description = QString::fromStdString(var->_description);
	std::lock_guard<std::mutex> lock(var->_lastValuesMutex);
	if (!var->_lastValues.empty()) {
		if (!description.isEmpty()) {
			description += QString("\n\n");
		}
		description +=
			QString(obs_module_text(
				"AdvSceneSwitcher.tempVar.selectionInfo.lastValues")) +
			"\n";
		for (const auto &value : var->_lastValues) {
			description += "\n" + QString::fromStdString(value);
		}
	}
	_info->setToolTip(description);
	_info->setVisible(!description.isEmpty());
	return description;
}

MacroSegment *TempVariableSelection::GetSegment() const
{
	const QWidget *widget = this;
	{
		while (widget) {
			if (qobject_cast<const MacroSegmentEdit *>(widget)) {
				break;
			}
			widget = widget->parentWidget();
		}
	}
	if (!widget) {
		return nullptr;
	}
	auto segmentWidget = qobject_cast<const MacroSegmentEdit *>(widget);
	return segmentWidget->Data().get();
}

void NotifyUIAboutTempVarChange()
{
	obs_queue_task(
		OBS_TASK_UI,
		[](void *) {
			if (!SettingsWindowIsOpened()) {
				return;
			}
			AdvSceneSwitcher::window->SegmentTempVarsChanged();
		},
		nullptr, false);
}

AutoUpdateTooltipLabel::AutoUpdateTooltipLabel(
	QWidget *parent, std::function<QString()> updateFunc)
	: QLabel(parent),
	  _updateFunc(updateFunc),
	  _timer(new QTimer(this))
{
	connect(_timer, &QTimer::timeout, this,
		&AutoUpdateTooltipLabel::UpdateTooltip);
}

#if QT_VERSION > QT_VERSION_CHECK(6, 0, 0)
void AutoUpdateTooltipLabel::enterEvent(QEnterEvent *event)
#else
void AutoUpdateTooltipLabel::enterEvent(QEvent *event)
#endif
{
	_timer->start(300);
	QLabel::enterEvent(event);
}

void AutoUpdateTooltipLabel::leaveEvent(QEvent *event)
{
	_timer->stop();
	QLabel::leaveEvent(event);
}

void AutoUpdateTooltipLabel::UpdateTooltip()
{
	setToolTip(_updateFunc());
}

} // namespace advss
