#include "variable-color-button.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QColorDialog>
#include <QHBoxLayout>

namespace advss {

VariableColorButton::VariableColorButton(QWidget *parent,
					 const QString &selectText)
	: QWidget(parent),
	  _colorSwatch(new QLabel()),
	  _selectColor(new QPushButton(selectText)),
	  _variable(new VariableSelection(this)),
	  _toggleType(new QPushButton())
{
	_toggleType->setCheckable(true);
	_toggleType->setMaximumWidth(11);
	SetButtonIcon(_toggleType, GetThemeTypeName() == "Light"
					   ? ":/res/images/dots-vert.svg"
					   : "theme:Dark/dots-vert.svg");

	QWidget::connect(_selectColor, SIGNAL(clicked()), this,
			 SLOT(SelectColorClicked()));
	QWidget::connect(_toggleType, SIGNAL(toggled(bool)), this,
			 SLOT(ToggleTypeClicked(bool)));
	QWidget::connect(_variable, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_colorSwatch);
	layout->addWidget(_selectColor);
	layout->addWidget(_variable);
	layout->addWidget(_toggleType);
	setLayout(layout);
	SetVisibility();
}

void VariableColorButton::SetValue(const ColorVariable &color)
{
	_color = color;
	const QSignalBlocker b1(_toggleType);
	const QSignalBlocker b2(_variable);
	_toggleType->setChecked(!color.IsFixedType());
	SetupColorLabel(color.GetFixedValue());
	_variable->SetVariable(color.GetVariable());
	SetVisibility();
}

void VariableColorButton::SelectColorClicked()
{
	const QColor color = QColorDialog::getColor(
		_color.GetFixedValue(), this, _selectColor->text(),
		QColorDialog::ColorDialogOption());
	if (!color.isValid()) {
		return;
	}
	SetupColorLabel(color);
	_color._value = color;
	emit ColorVariableChanged(_color);
}

void VariableColorButton::VariableChanged(const QString &name)
{
	_color._variable = GetWeakVariableByQString(name);
	emit ColorVariableChanged(_color);
}

void VariableColorButton::ToggleTypeClicked(bool useVariable)
{
	_color._type = useVariable ? ColorVariable::Type::VARIABLE
				   : ColorVariable::Type::FIXED_VALUE;
	SetVisibility();
	emit ColorVariableChanged(_color);
}

void VariableColorButton::SetupColorLabel(const QColor &color)
{
	_colorSwatch->setText(color.name());
	_colorSwatch->setStyleSheet(
		QString("background-color: %1;").arg(color.name()));
}

void VariableColorButton::SetVisibility()
{
	if (_color.IsFixedType()) {
		SetupColorLabel(_color.GetFixedValue());
		_colorSwatch->show();
		_selectColor->show();
		_variable->hide();
		_toggleType->setVisible(!GetVariables().empty());
	} else {
		_colorSwatch->hide();
		_selectColor->hide();
		_variable->show();
		_variable->setToolTip(obs_module_text(
			"AdvSceneSwitcher.condition.video.colorVariableTooltip"));
		_toggleType->show();
	}
	adjustSize();
	updateGeometry();
}

} // namespace advss
