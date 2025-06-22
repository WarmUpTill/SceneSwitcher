#include "macro-segment.hpp"
#include "macro.hpp"
#include "mouse-wheel-guard.hpp"
#include "section.hpp"
#include "ui-helpers.hpp"

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>

namespace advss {

MacroSegment::MacroSegment(Macro *m, bool supportsVariableValue)
	: _macro(m),
	  _supportsVariableValue(supportsVariableValue)
{
}

bool MacroSegment::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "collapsed", _collapsed);
	obs_data_set_bool(data, "useCustomLabel", _useCustomLabel);
	obs_data_set_string(data, "customLabel", _customLabel.c_str());
	obs_data_set_bool(data, "enabled", _enabled);
	obs_data_set_int(data, "version", 1);
	obs_data_set_obj(obj, "segmentSettings", data);
	return true;
}

bool MacroSegment::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "segmentSettings");
	_collapsed = obs_data_get_bool(data, "collapsed");
	_useCustomLabel = obs_data_get_bool(data, "useCustomLabel");
	_customLabel = obs_data_get_string(data, "customLabel");
	obs_data_set_default_bool(data, "enabled", true);
	_enabled = obs_data_get_bool(data, "enabled");

	// TODO: remove this fallback at some point
	if (!obs_data_has_user_value(data, "version")) {
		_enabled = obs_data_get_bool(obj, "enabled");
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
	  _noBorderframe(new QFrame),
	  _borderFrame(new QFrame),
	  _dropLineAbove(new QFrame),
	  _dropLineBelow(new QFrame)
{
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
	// Macro signals
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(MacroAdded(const QString &)), this,
			 SIGNAL(MacroAdded(const QString &)));
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(MacroRemoved(const QString &)), this,
			 SIGNAL(MacroRemoved(const QString &)));
	QWidget::connect(
		GetSettingsWindow(),
		SIGNAL(MacroRenamed(const QString &, const QString &)), this,
		SIGNAL(MacroRenamed(const QString &, const QString &)));
	// Scene group signals
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(SceneGroupAdded(const QString &)), this,
			 SIGNAL(SceneGroupAdded(const QString &)));
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(SceneGroupRemoved(const QString &)), this,
			 SIGNAL(SceneGroupRemoved(const QString &)));
	QWidget::connect(
		GetSettingsWindow(),
		SIGNAL(SceneGroupRenamed(const QString &, const QString)), this,
		SIGNAL(SceneGroupRenamed(const QString &, const QString)));

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
