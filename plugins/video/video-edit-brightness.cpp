#include "macro-condition-video.hpp"

#include <QTimer>
#include <QVBoxLayout>

namespace advss {

BrightnessEdit::BrightnessEdit(QWidget *parent,
			       const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _threshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.brightnessThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.brightnessThresholdDescription"))),
	  _current(new QLabel),
	  _entryData(data)
{
	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_threshold);
	layout->addWidget(_current);
	setLayout(layout);

	QWidget::connect(
		_threshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(BrightnessThresholdChanged(
			const NumberVariable<double> &)));
	QWidget::connect(&_timer, &QTimer::timeout, this,
			 &BrightnessEdit::UpdateCurrentBrightness);
	_timer.start(1000);

	_threshold->SetDoubleValue(_entryData->_brightnessThreshold);
	_loading = false;
}

void BrightnessEdit::UpdateCurrentBrightness()
{
	QString text = obs_module_text(
		"AdvSceneSwitcher.condition.video.currentBrightness");
	_current->setText(text.arg(_entryData->GetCurrentBrightness()));
}

void BrightnessEdit::BrightnessThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_brightnessThreshold = value;
}

} // namespace advss
