#include "macro-segment.hpp"
#include "macro.hpp"
#include "mouse-wheel-guard.hpp"
#include "path-helpers.hpp"
#include "section.hpp"
#include "ui-helpers.hpp"
#include "variable.hpp"

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>

namespace advss {

std::vector<TempVariableRef> MacroSegment::GetTempVarRefs() const
{
	std::vector<TempVariableRef> refs;
	for (const auto &mapping : _varMappings) {
		if (mapping.tempVar.HasValidID()) {
			refs.push_back(mapping.tempVar);
		}
	}
	return refs;
}

void MacroSegment::ApplyVarMappings()
{
	auto macro = GetMacro();
	for (const auto &mapping : _varMappings) {
		auto var = mapping.variable.lock();
		if (!var) {
			continue;
		}
		const auto tempVar = mapping.tempVar.GetTempVariable(macro);
		if (!tempVar) {
			continue;
		}
		const auto value = tempVar->Value();
		if (!value) {
			continue;
		}
		var->SetValue(*value);
	}
}

const std::vector<VarMapping> &MacroSegment::GetVarMappings() const
{
	return _varMappings;
}

void MacroSegment::SetVarMappings(const std::vector<VarMapping> &mappings)
{
	_varMappings = mappings;
}

std::vector<TempVariable> MacroSegment::GetOwnTempVars() const
{
	return _tempVariables;
}

MacroSegment::MacroSegment(Macro *m, bool supportsVariableValue)
	: _macro(m),
	  _supportsVariableValue(supportsVariableValue)
{
}

void MacroSegment::SetVarMappingExpanded(bool expanded)
{
	_varMappingExpanded = expanded;
}

bool MacroSegment::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "collapsed", _collapsed);
	obs_data_set_bool(data, "varMappingExpanded", _varMappingExpanded);
	obs_data_set_bool(data, "useCustomLabel", _useCustomLabel);
	obs_data_set_string(data, "customLabel", _customLabel.c_str());
	obs_data_set_bool(data, "enabled", _enabled);
	obs_data_set_int(data, "version", 2);

	OBSDataArrayAutoRelease mappingsArray = obs_data_array_create();
	for (const auto &mapping : _varMappings) {
		OBSDataAutoRelease item = obs_data_create();
		mapping.tempVar.Save(item, GetMacro(), "tempVar");
		obs_data_set_string(
			item, "variable",
			GetWeakVariableName(mapping.variable).c_str());
		obs_data_array_push_back(mappingsArray, item);
	}
	obs_data_set_array(data, "varMappings", mappingsArray);

	obs_data_set_obj(obj, "segmentSettings", data);
	return true;
}

bool MacroSegment::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "segmentSettings");
	_collapsed = obs_data_get_bool(data, "collapsed");
	_varMappingExpanded = obs_data_get_bool(data, "varMappingExpanded");
	_useCustomLabel = obs_data_get_bool(data, "useCustomLabel");
	_customLabel = obs_data_get_string(data, "customLabel");
	obs_data_set_default_bool(data, "enabled", true);
	_enabled = obs_data_get_bool(data, "enabled");

	// TODO: remove this fallback at some point
	if (!obs_data_has_user_value(data, "version")) {
		_enabled = obs_data_get_bool(obj, "enabled");
	}

	// Reset the previously unused "enabled" value for conditions to "true"
	if (obs_data_get_int(data, "version") < 2 &&
	    obs_data_has_user_value(obj, "logic")) {
		_enabled = true;
	}

	_varMappings.clear();
	OBSDataArrayAutoRelease mappingsArray =
		obs_data_get_array(data, "varMappings");
	const size_t count = obs_data_array_count(mappingsArray);
	_varMappings.reserve(count);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(mappingsArray, i);
		VarMapping mapping;
		mapping.variable = GetWeakVariableByName(
			obs_data_get_string(item, "variable"));
		_varMappings.push_back(std::move(mapping));
		_varMappings.back().tempVar.Load(item, GetMacro(), "tempVar");
	}

	ClearAvailableTempvars();
	return true;
}

bool MacroSegment::PostLoad()
{
	SetupTempVars();
	return true;
}

std::string MacroSegment::GetShortDesc() const
{
	return "";
}

void MacroSegment::EnableHighlight()
{
	_highlight = true;
}

bool MacroSegment::GetHighlightAndReset()
{
	if (_highlight) {
		_highlight = false;
		return true;
	}
	return false;
}

void MacroSegment::SetEnabled(bool value)
{
	_enabled = value;
}

bool MacroSegment::Enabled() const
{
	return _enabled;
}

std::string MacroSegment::GetVariableValue() const
{
	if (_supportsVariableValue) {
		return _variableValue;
	}
	return "";
}

void MacroSegment::SetVariableValue(const std::string &value)
{
	if (_variableRefs > 0) {
		_variableValue = value;
	}
}

void MacroSegment::SetupTempVars()
{
	ClearAvailableTempvars();
}

void MacroSegment::ClearAvailableTempvars()
{
	_tempVariables.clear();
	NotifyUIAboutTempVarChange(this);
}

static std::shared_ptr<MacroSegment>
getSharedSegmentFromMacro(Macro *macro, const MacroSegment *segment)
{
	auto matches = [segment](const std::shared_ptr<MacroSegment> &s) {
		return s.get() == segment;
	};
	auto condIt = std::find_if(macro->Conditions().begin(),
				   macro->Conditions().end(), matches);
	if (condIt != macro->Conditions().end()) {
		return *condIt;
	}
	auto actionIt = std::find_if(macro->Actions().begin(),
				     macro->Actions().end(), matches);
	if (actionIt != macro->Actions().end()) {
		return *actionIt;
	}
	auto elseIt = std::find_if(macro->ElseActions().begin(),
				   macro->ElseActions().end(), matches);
	if (elseIt != macro->ElseActions().end()) {
		return *elseIt;
	}
	return {};
}

void MacroSegment::AddTempvar(const std::string &id, const std::string &name,
			      const std::string &description)
{
	auto macro = GetMacro();
	if (!macro) {
		return;
	}

	auto sharedSegment = getSharedSegmentFromMacro(macro, this);
	if (!sharedSegment) {
		return;
	}

	TempVariable var(id, name, description, sharedSegment);
	_tempVariables.emplace_back(std::move(var));
	NotifyUIAboutTempVarChange(this);
}

bool MacroSegment::IsTempVarInUse(const std::string &id) const
{
	for (const auto &var : _tempVariables) {
		if (var.ID() == id) {
			return var.IsInUse();
		}
	}
	return false;
}

void MacroSegment::SetTempVarValue(const std::string &id,
				   const std::string &value)
{
	for (auto &var : _tempVariables) {
		if (var.ID() != id) {
			continue;
		}
		var.SetValue(value);
		break;
	}
}

void MacroSegment::InvalidateTempVarValues()
{
	for (auto &var : _tempVariables) {
		var.InvalidateValue();
	}
}

std::optional<const TempVariable>
MacroSegment::GetTempVar(const std::string &id) const
{
	for (auto &var : _tempVariables) {
		if (var.ID() == id) {
			return var;
		}
	}
	vblog(LOG_INFO, "cannot get value of unknown tempvar %s", id.c_str());
	return {};
}

bool SupportsVariableValue(MacroSegment *segment)
{
	return segment && segment->_supportsVariableValue;
}

void IncrementVariableRef(MacroSegment *segment)
{
	if (!segment || !segment->_supportsVariableValue) {
		return;
	}
	segment->_variableRefs++;
}

void DecrementVariableRef(MacroSegment *segment)
{
	if (!segment || !segment->_supportsVariableValue ||
	    segment->_variableRefs == 0) {
		return;
	}
	segment->_variableRefs--;
}

MacroSegmentEdit::MacroSegmentEdit(QWidget *parent)
	: QWidget(parent),
	  _section(new Section(300)),
	  _headerInfo(new QLabel()),
	  _frame(new QWidget),
	  _contentLayout(new QVBoxLayout),
	  _outputMappings(new TempVarOutputMappingsWidget(this)),
	  _varMappingToggle(new QPushButton(this)),
	  _noBorderframe(new QFrame),
	  _borderFrame(new QFrame),
	  _dropLineAbove(new QFrame),
	  _dropLineBelow(new QFrame)
{
	const auto iconPath = QString::fromStdString(GetDataFilePath(
		"res/images/" + GetThemeTypeName() + "Variable.svg"));
	_varMappingToggle->setIcon(QIcon(iconPath));
	_varMappingToggle->setMaximumWidth(22);
	_varMappingToggle->setFlat(true);
	_varMappingToggle->setCheckable(true);
	_varMappingToggle->setVisible(false);
	_varMappingToggle->setToolTip(obs_module_text(
		"AdvSceneSwitcher.tempVar.outputMappings.toggle"));

	QWidget::connect(_varMappingToggle, &QPushButton::toggled,
			 _outputMappings,
			 &TempVarOutputMappingsWidget::SetPanelExpanded);
	QWidget::connect(_varMappingToggle, &QPushButton::toggled, this,
			 [this](bool checked) {
				 if (Data()) {
					 Data()->SetVarMappingExpanded(checked);
				 }
			 });
	QWidget::connect(_outputMappings,
			 &TempVarOutputMappingsWidget::ExpandableChanged,
			 _varMappingToggle, &QPushButton::setVisible);
	QWidget::connect(_section, &Section::Collapsed, _outputMappings,
			 &TempVarOutputMappingsWidget::SetSectionCollapsed);

	_dropLineAbove->setLineWidth(3);
	_dropLineAbove->setFixedHeight(11);
	_dropLineBelow->setLineWidth(3);
	_dropLineBelow->setFixedHeight(11);

	_borderFrame->setObjectName("border");
	_borderFrame->setStyleSheet("#border {"
				    "border-color: rgba(0, 0, 0, 255);"
				    "border-width: 2px;"
				    "border-style: dashed;"
				    "border-radius: 4px;"
				    "background-color: rgba(0,0,0,100);"
				    "}");
	_noBorderframe->setObjectName("noBorder");
	_noBorderframe->setStyleSheet("#noBorder {"
				      "border-color: rgba(0, 0, 0, 0);"
				      "border-width: 2px;"
				      "border-style: dashed;"
				      "border-radius: 4px;"
				      "background-color: rgba(0,0,0,50);"
				      "}");
	_frame->setObjectName("frameWrapper");
	_frame->setStyleSheet("#frameWrapper {"
			      "border-width: 2px;"
			      "border-radius: 4px;"
			      "background-color: rgba(0,0,0,0);"
			      "}");

	// Set background of these widget types to be transparent to avoid
	// blocking highlight frame background
	setStyleSheet("QCheckBox { background-color: rgba(0,0,0,0); }"
		      "QLabel { background-color: rgba(0,0,0,0); }"
		      "QSlider { background-color: rgba(0,0,0,0); }");

	// Keep the size of macro segments consistent, even if there is room in
	// the edit areas
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QWidget::connect(_section, &Section::Collapsed, this,
			 &MacroSegmentEdit::Collapsed);

	auto frameLayout = new QGridLayout;
	frameLayout->setContentsMargins(0, 0, 0, 0);
	frameLayout->addLayout(_contentLayout, 0, 0);
	frameLayout->addWidget(_noBorderframe, 0, 0);
	frameLayout->addWidget(_borderFrame, 0, 0);
	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(_dropLineAbove);
	layout->addLayout(frameLayout);
	layout->addWidget(_dropLineBelow);
	_frame->setLayout(layout);

	SetSelected(false);
	ShowDropLine(DropLineState::NONE);

	// Enable dragging while clicking on the header text
	_headerInfo->installEventFilter(this);
}

void MacroSegmentEdit::SetupVarMappings(MacroSegment *segment)
{
	_outputMappings->SetSegment(segment);
	_varMappingToggle->setChecked(segment &&
				      segment->GetVarMappingExpanded());
}

void MacroSegmentEdit::ShowVariableMappings(bool show)
{
	_varMappingToggle->setChecked(show);
	if (Data()) {
		Data()->SetVarMappingExpanded(show);
	}
}

bool MacroSegmentEdit::eventFilter(QObject *obj, QEvent *ev)
{
	if (obj == _headerInfo && ev->type() == QEvent::MouseMove) {
		if (!parentWidget()) {
			return QWidget::eventFilter(obj, ev);
		}
		// Generate a mouse move event for the MacroSegmentList
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(ev);
		QMouseEvent *newEvent = new QMouseEvent(
			mouseEvent->type(),
			_headerInfo->mapTo(this, mouseEvent->pos()),
			mouseEvent->globalPosition(), mouseEvent->button(),
			mouseEvent->buttons(), mouseEvent->modifiers());
		QApplication::sendEvent(parentWidget(), newEvent);
	}
	return QWidget::eventFilter(obj, ev);
}

void MacroSegmentEdit::HeaderInfoChanged(const QString &text)
{
	if (Data() && Data()->GetUseCustomLabel()) {
		_headerInfo->show();
		_headerInfo->setText(
			QString::fromStdString(Data()->GetCustomLabel()));
		return;
	}
	_headerInfo->setVisible(!text.isEmpty());
	_headerInfo->setText(text);
}

void MacroSegmentEdit::Collapsed(bool collapsed) const
{
	if (Data()) {
		Data()->SetCollapsed(collapsed);
	}
}

void MacroSegmentEdit::SetDisableEffect(bool value)
{
	if (value) {
		auto effect = new QGraphicsOpacityEffect(this);
		effect->setOpacity(0.5);
		_section->setGraphicsEffect(effect);
	} else {
		_section->setGraphicsEffect(nullptr);
	}
}

void MacroSegmentEdit::SetEnableAppearance(bool value)
{
	SetDisableEffect(!value);
}

void MacroSegmentEdit::SetFocusPolicyOfWidgets()
{
	QList<QWidget *> widgets = this->findChildren<QWidget *>();
	for (auto w : widgets) {
		PreventMouseWheelAdjustWithoutFocus(w);
	}
}

void MacroSegmentEdit::SetCollapsed(bool collapsed)
{
	_section->SetCollapsed(collapsed);
}

void MacroSegmentEdit::SetSelected(bool selected)
{
	_borderFrame->setVisible(selected);
	_noBorderframe->setVisible(!selected);
}

void MacroSegmentEdit::ShowDropLine(DropLineState state)
{
	switch (state) {
	case MacroSegmentEdit::DropLineState::NONE:
		_dropLineAbove->setFrameShadow(QFrame::Plain);
		_dropLineAbove->setFrameShape(QFrame::NoFrame);
		_dropLineBelow->hide();
		break;
	case MacroSegmentEdit::DropLineState::ABOVE:
		_dropLineAbove->setFrameShadow(QFrame::Sunken);
		_dropLineAbove->setFrameShape(QFrame::Panel);
		_dropLineBelow->hide();
		break;
	case MacroSegmentEdit::DropLineState::BELOW:
		_dropLineAbove->setFrameShadow(QFrame::Plain);
		_dropLineAbove->setFrameShape(QFrame::NoFrame);
		_dropLineBelow->setFrameShadow(QFrame::Sunken);
		_dropLineBelow->setFrameShape(QFrame::Panel);
		_dropLineBelow->show();
		break;
	default:
		break;
	}
}

} // namespace advss
