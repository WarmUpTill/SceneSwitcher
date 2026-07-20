#include "macro-condition-video.hpp"

#include <layout-helpers.hpp>
#include <plugin-state-helpers.hpp>
#include <ui-helpers.hpp>

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace advss {

static QDoubleSpinBox *makeDoubleSpinBox(double min, double max,
					 double singleStep, int decimals,
					 double value, QWidget *parent)
{
	auto *spin = new QDoubleSpinBox(parent);
	spin->setMinimum(min);
	spin->setMaximum(max);
	spin->setSingleStep(singleStep);
	spin->setDecimals(decimals);
	spin->setValue(value);
	return spin;
}

DnnObjectDetectEdit::DnnObjectDetectEdit(
	QWidget *parent, PreviewDialog *previewDialog,
	const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _modelDataPath(new FileSelection()),
	  _configPath(new FileSelection()),
	  _confidenceThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.confidenceThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.confidenceThresholdDescription"))),
	  _nmsThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.nmsThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.nmsThresholdDescription"))),
	  _inputSize(new SizeSelection(1, 4096)),
	  _scaleFactor(makeDoubleSpinBox(0.0, 1.0, 0.0001, 6,
					 data->_dnnMatchParameters.scaleFactor,
					 this)),
	  _meanR(makeDoubleSpinBox(0.0, 255.0, 0.5, 1,
				   data->_dnnMatchParameters.meanR, this)),
	  _meanG(makeDoubleSpinBox(0.0, 255.0, 0.5, 1,
				   data->_dnnMatchParameters.meanG, this)),
	  _meanB(makeDoubleSpinBox(0.0, 255.0, 0.5, 1,
				   data->_dnnMatchParameters.meanB, this)),
	  _swapRB(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.dnnSwapRB"))),
	  _previewDialog(previewDialog),
	  _entryData(data)
{
	QWidget::connect(_modelDataPath, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ModelPathChanged(const QString &)));
	QWidget::connect(_configPath, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ConfigPathChanged(const QString &)));
	QWidget::connect(
		_confidenceThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ConfidenceThresholdChanged(
			const NumberVariable<double> &)));
	QWidget::connect(
		_nmsThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(NmsThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(_inputSize, SIGNAL(SizeChanged(Size)), this,
			 SLOT(InputSizeChanged(Size)));
	QWidget::connect(_scaleFactor, SIGNAL(valueChanged(double)), this,
			 SLOT(ScaleFactorChanged(double)));
	QWidget::connect(_meanR, SIGNAL(valueChanged(double)), this,
			 SLOT(MeanRChanged(double)));
	QWidget::connect(_meanG, SIGNAL(valueChanged(double)), this,
			 SLOT(MeanGChanged(double)));
	QWidget::connect(_meanB, SIGNAL(valueChanged(double)), this,
			 SLOT(MeanBChanged(double)));
	QWidget::connect(_swapRB, SIGNAL(stateChanged(int)), this,
			 SLOT(SwapRBChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{modelDataPath}}", _modelDataPath},
		{"{{configPath}}", _configPath},
	};

	auto pathLayout = new QHBoxLayout;
	pathLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.dnnModelPath"),
		pathLayout, widgetPlaceholders);

	auto configPathLayout = new QHBoxLayout;
	configPathLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.dnnConfigPath"),
		configPathLayout, widgetPlaceholders);

	auto inputSizeLayout = new QHBoxLayout;
	inputSizeLayout->setContentsMargins(0, 0, 0, 0);
	inputSizeLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.dnnInputSize")));
	inputSizeLayout->addWidget(_inputSize);
	inputSizeLayout->addStretch();

	auto scaleFactorLayout = new QHBoxLayout;
	scaleFactorLayout->setContentsMargins(0, 0, 0, 0);
	scaleFactorLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.dnnScaleFactor")));
	scaleFactorLayout->addWidget(_scaleFactor);
	scaleFactorLayout->addStretch();

	auto meanLayout = new QHBoxLayout;
	meanLayout->setContentsMargins(0, 0, 0, 0);
	meanLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.video.dnnMean")));
	meanLayout->addWidget(_meanR);
	meanLayout->addWidget(_meanG);
	meanLayout->addWidget(_meanB);
	meanLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(pathLayout);
	layout->addLayout(configPathLayout);
	layout->addWidget(_confidenceThreshold);
	layout->addWidget(_nmsThreshold);
	layout->addLayout(inputSizeLayout);
	layout->addLayout(scaleFactorLayout);
	layout->addLayout(meanLayout);
	layout->addWidget(_swapRB);
	setLayout(layout);

	_modelDataPath->SetPath(_entryData->_dnnMatchParameters.GetModelPath());
	_configPath->SetPath(_entryData->_dnnMatchParameters.GetConfigPath());
	_confidenceThreshold->SetDoubleValue(
		_entryData->_dnnMatchParameters.confidenceThreshold);
	_nmsThreshold->SetDoubleValue(
		_entryData->_dnnMatchParameters.nmsThreshold);
	_inputSize->SetSize(_entryData->_dnnMatchParameters.inputSize);
	_swapRB->setChecked(_entryData->_dnnMatchParameters.swapRB);
	_loading = false;
}

void DnnObjectDetectEdit::ModelPathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	bool dataLoaded = false;
	{
		auto lock = LockContext();
		std::string path = text.toStdString();
		dataLoaded = _entryData->_dnnMatchParameters.SetModelPath(path);
	}
	if (!dataLoaded) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.modelLoadFail"));
	}
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::ConfigPathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	bool dataLoaded = false;
	{
		auto lock = LockContext();
		std::string path = text.toStdString();
		dataLoaded =
			_entryData->_dnnMatchParameters.SetConfigPath(path);
	}
	if (!dataLoaded) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.modelLoadFail"));
	}
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::ConfidenceThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.confidenceThreshold = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::NmsThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.nmsThreshold = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::InputSizeChanged(Size value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.inputSize = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::ScaleFactorChanged(double value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.scaleFactor = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::MeanRChanged(double value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.meanR = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::MeanGChanged(double value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.meanG = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::MeanBChanged(double value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.meanB = value;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

void DnnObjectDetectEdit::SwapRBChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_dnnMatchParameters.swapRB = value != Qt::Unchecked;
	_previewDialog->DnnDetectParametersChanged(
		_entryData->_dnnMatchParameters);
}

} // namespace advss
