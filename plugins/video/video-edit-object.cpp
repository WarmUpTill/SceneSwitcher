#include "macro-condition-video.hpp"

#include <layout-helpers.hpp>
#include <plugin-state-helpers.hpp>
#include <ui-helpers.hpp>

#include <QSpinBox>
#include <QVBoxLayout>

namespace advss {

ObjectDetectEdit::ObjectDetectEdit(
	QWidget *parent, PreviewDialog *previewDialog,
	const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _modelDataPath(new FileSelection()),
	  _objectScaleThreshold(new SliderSpinBox(
		  1.1, 5.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.objectScaleThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.objectScaleThresholdDescription"))),
	  _minNeighbors(new QSpinBox()),
	  _minNeighborsDescription(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.video.minNeighborDescription"))),
	  _minSize(new SizeSelection(0, 1024)),
	  _maxSize(new SizeSelection(0, 4096)),
	  _previewDialog(previewDialog),
	  _entryData(data)
{
	_minNeighbors->setMinimum(minMinNeighbors);
	_minNeighbors->setMaximum(maxMinNeighbors);

	QWidget::connect(
		_objectScaleThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ObjectScaleThresholdChanged(
			const NumberVariable<double> &)));
	QWidget::connect(_minNeighbors, SIGNAL(valueChanged(int)), this,
			 SLOT(MinNeighborsChanged(int)));
	QWidget::connect(_minSize, SIGNAL(SizeChanged(Size)), this,
			 SLOT(MinSizeChanged(Size)));
	QWidget::connect(_maxSize, SIGNAL(SizeChanged(Size)), this,
			 SLOT(MaxSizeChanged(Size)));
	QWidget::connect(_modelDataPath, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ModelPathChanged(const QString &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{minNeighbors}}", _minNeighbors},
		{"{{minSize}}", _minSize},
		{"{{maxSize}}", _maxSize},
		{"{{modelDataPath}}", _modelDataPath},
	};

	auto pathLayout = new QHBoxLayout;
	pathLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.modelPath"),
		pathLayout, widgetPlaceholders);

	auto neighborsLayout = new QHBoxLayout;
	neighborsLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.minNeighbor"),
		neighborsLayout, widgetPlaceholders);

	auto sizeGrid = new QGridLayout;
	sizeGrid->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.condition.video.minSize")),
		0, 0);
	sizeGrid->addWidget(_minSize, 0, 1);
	sizeGrid->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.condition.video.maxSize")),
		1, 0);
	sizeGrid->addWidget(_maxSize, 1, 1);
	auto sizeLayout = new QHBoxLayout;
	sizeLayout->setContentsMargins(0, 0, 0, 0);
	sizeLayout->addLayout(sizeGrid);
	sizeLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(pathLayout);
	layout->addLayout(neighborsLayout);
	layout->addLayout(sizeLayout);
	setLayout(layout);

	_modelDataPath->SetPath(_entryData->_objMatchParameters.GetModelPath());
	_objectScaleThreshold->SetDoubleValue(
		_entryData->_objMatchParameters.scaleFactor);
	_minNeighbors->setValue(_entryData->_objMatchParameters.minNeighbors);
	_minSize->SetSize(_entryData->_objMatchParameters.minSize);
	_maxSize->SetSize(_entryData->_objMatchParameters.maxSize);
	_loading = false;
}

void ObjectDetectEdit::ObjectScaleThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_objMatchParameters.scaleFactor = value;
	_previewDialog->ObjDetectParametersChanged(
		_entryData->_objMatchParameters);
}

void ObjectDetectEdit::MinNeighborsChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_objMatchParameters.minNeighbors = value;
	_previewDialog->ObjDetectParametersChanged(
		_entryData->_objMatchParameters);
}

void ObjectDetectEdit::MinSizeChanged(advss::Size value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_objMatchParameters.minSize = value;
	_previewDialog->ObjDetectParametersChanged(
		_entryData->_objMatchParameters);
}

void ObjectDetectEdit::MaxSizeChanged(advss::Size value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_objMatchParameters.maxSize = value;
	_previewDialog->ObjDetectParametersChanged(
		_entryData->_objMatchParameters);
}

void ObjectDetectEdit::ModelPathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	bool dataLoaded = false;
	{
		auto lock = LockContext();
		std::string path = text.toStdString();
		dataLoaded = _entryData->_objMatchParameters.SetModelPath(path);
	}
	if (!dataLoaded) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.modelLoadFail"));
	}
	_previewDialog->ObjDetectParametersChanged(
		_entryData->_objMatchParameters);
}

} // namespace advss
