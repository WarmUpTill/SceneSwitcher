#include "macro-condition-video.hpp"

#include <layout-helpers.hpp>
#include <plugin-state-helpers.hpp>

#include <QPushButton>

namespace advss {

AreaEdit::AreaEdit(QWidget *parent, PreviewDialog *previewDialog,
		   const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _checkAreaEnable(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.layout.checkAreaEnable"))),
	  _checkArea(new AreaSelection(0, 99999)),
	  _selectArea(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectArea"))),
	  _previewDialog(previewDialog),
	  _entryData(data)
{
	QWidget::connect(_checkAreaEnable, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckAreaEnableChanged(int)));
	QWidget::connect(_checkArea, SIGNAL(AreaChanged(Area)), this,
			 SLOT(CheckAreaChanged(Area)));
	QWidget::connect(_selectArea, SIGNAL(clicked()), this,
			 SLOT(SelectAreaClicked()));
	QWidget::connect(_previewDialog, SIGNAL(SelectionAreaChanged(QRect)),
			 this, SLOT(CheckAreaChanged(QRect)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{checkAreaEnable}}", _checkAreaEnable},
		{"{{checkArea}}", _checkArea},
		{"{{selectArea}}", _selectArea},
	};

	auto layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.checkArea"),
		layout, widgetPlaceholders);
	setLayout(layout);

	_checkAreaEnable->setChecked(_entryData->_areaParameters.enable);
	_checkArea->SetArea(_entryData->_areaParameters.area);
	SetWidgetVisibility();
	_loading = false;
}

void AreaEdit::SetWidgetVisibility()
{
	_checkArea->setVisible(_entryData->_areaParameters.enable);
	_selectArea->setVisible(_entryData->_areaParameters.enable);
	adjustSize();
	updateGeometry();
}

void AreaEdit::SelectAreaClicked()
{
	_previewDialog->show();
	_previewDialog->raise();
	_previewDialog->activateWindow();
	_previewDialog->SelectArea();
}

void AreaEdit::CheckAreaEnableChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_areaParameters.enable = value;
	SetWidgetVisibility();
	_previewDialog->AreaParametersChanged(_entryData->_areaParameters);
	emit Resized();
}

void AreaEdit::CheckAreaChanged(Area value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_areaParameters.area = value;
	_previewDialog->AreaParametersChanged(_entryData->_areaParameters);
}

void AreaEdit::CheckAreaChanged(QRect rect)
{
	const QSignalBlocker b(_checkArea);
	Area area{rect.topLeft().x(), rect.y(), rect.width(), rect.height()};
	_checkArea->SetArea(area);
	CheckAreaChanged(area);
}

} // namespace advss
