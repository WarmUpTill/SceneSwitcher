#include "area-selection.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <obs-module.h>

void advss::Size::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	width.Save(data, "width");
	height.Save(data, "height");
	obs_data_set_int(data, "version", 1);
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

void advss::Size::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	if (!obs_data_has_user_value(data, "version")) {
		width = obs_data_get_int(data, "width");
		height = obs_data_get_int(data, "height");
	} else {
		width.Load(data, "width");
		height.Load(data, "height");
	}
	obs_data_release(data);
}

cv::Size advss::Size::CV()
{
	return {width, height};
}

void advss::Area::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	x.Save(data, "x");
	y.Save(data, "y");
	width.Save(data, "width");
	height.Save(data, "height");
	obs_data_set_int(data, "version", 1);
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

void advss::Area::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	if (!obs_data_has_user_value(data, "version")) {
		x = obs_data_get_int(data, "x");
		y = obs_data_get_int(data, "y");
		width = obs_data_get_int(data, "width");
		height = obs_data_get_int(data, "height");
	} else {
		x.Load(data, "x");
		y.Load(data, "y");
		width.Load(data, "width");
		height.Load(data, "height");
	}
	obs_data_release(data);
}

SizeSelection::SizeSelection(int min, int max, QWidget *parent)
	: QWidget(parent), _x(new VariableSpinBox()), _y(new VariableSpinBox())
{
	_x->setMinimum(min);
	_y->setMinimum(min);
	_x->setMaximum(max);
	_y->setMaximum(max);

	connect(_x, SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(XChanged(const NumberVariable<int> &)));
	connect(_y, SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(YChanged(const NumberVariable<int> &)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_x);
	layout->addWidget(_y);
	setLayout(layout);
}

void SizeSelection::SetSize(const advss::Size &s)
{
	_x->SetValue(s.width);
	_y->SetValue(s.height);
}

advss::Size SizeSelection::Size()
{
	return advss::Size{_x->Value(), _y->Value()};
}

void SizeSelection::XChanged(const NumberVariable<int> &value)
{
	emit SizeChanged(advss::Size{value, _y->Value()});
}

void SizeSelection::YChanged(const NumberVariable<int> &value)
{
	emit SizeChanged(advss::Size{_x->Value(), value});
}

AreaSelection::AreaSelection(int min, int max, QWidget *parent)
	: QWidget(parent),
	  _x(new SizeSelection(min, max)),
	  _y(new SizeSelection(min, max))
{
	_x->_x->setPrefix("X:");
	_x->_y->setPrefix("Y:");
	_y->_x->setPrefix(QString(obs_module_text(
				  "AdvSceneSwitcher.condition.video.width")) +
			  ":");
	_y->_y->setPrefix(QString(obs_module_text(
				  "AdvSceneSwitcher.condition.video.height")) +
			  ":");

	connect(_x, SIGNAL(SizeChanged(advss::Size)), this,
		SLOT(XSizeChanged(advss::Size)));
	connect(_y, SIGNAL(SizeChanged(advss::Size)), this,
		SLOT(YSizeChanged(advss::Size)));

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_x);
	layout->addWidget(_y);
	setLayout(layout);
}

void AreaSelection::SetArea(const advss::Area &value)
{
	_x->SetSize({value.x, value.y});
	_y->SetSize({value.width, value.height});
}

void AreaSelection::XSizeChanged(advss::Size value)
{
	emit AreaChanged(advss::Area{value.width, value.height,
				     _y->Size().width, _y->Size().height});
}

void AreaSelection::YSizeChanged(advss::Size value)
{
	emit AreaChanged(advss::Area{_x->Size().width, _x->Size().height,
				     value.width, value.height});
}
