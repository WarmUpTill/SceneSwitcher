#include "macro-condition-video.hpp"

#include <layout-helpers.hpp>
#include <plugin-state-helpers.hpp>

#include <QVBoxLayout>

namespace advss {

ColorEdit::ColorEdit(QWidget *parent,
		     const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _matchThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorMatchThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorMatchThresholdDescription"),
		  true)),
	  _colorThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThresholdDescription"),
		  true)),
	  _colorButton(new VariableColorButton(
		  this,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.selectColor"))),
	  _entryData(data)
{
	QWidget::connect(_colorButton,
			 SIGNAL(ColorVariableChanged(const ColorVariable &)),
			 this, SLOT(ColorChanged(const ColorVariable &)));
	QWidget::connect(
		_matchThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(MatchThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(
		_colorThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ColorThresholdChanged(const NumberVariable<double> &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{color}}", _colorButton},
	};

	auto colorLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.video.layout.color"),
		     colorLayout, widgetPlaceholders);

	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(colorLayout);
	layout->addWidget(_colorThreshold);
	layout->addWidget(_matchThreshold);
	setLayout(layout);

	_matchThreshold->SetDoubleValue(
		_entryData->_colorParameters.matchThreshold);
	_colorThreshold->SetDoubleValue(
		_entryData->_colorParameters.colorThreshold);
	_colorButton->SetValue(_entryData->_colorParameters.color);
	_loading = false;
}

void ColorEdit::ColorChanged(const ColorVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_colorParameters.color = value;
}

void ColorEdit::MatchThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_colorParameters.matchThreshold = value;
}

void ColorEdit::ColorThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_colorParameters.colorThreshold = value;
}

} // namespace advss
