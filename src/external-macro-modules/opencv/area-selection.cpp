#include "area-selection.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <obs-module.h>

void advss::Size::Save(obs_data_t *obj, const char *name)
{
	auto data = obs_data_create();
	obs_data_set_int(data, "width", width);
	obs_data_set_int(data, "height", height);
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

void advss::Size::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	width = obs_data_get_int(data, "width");
	height = obs_data_get_int(data, "height");
	obs_data_release(data);
}

cv::Size advss::Size::CV()
{
	return {width, height};
}

void advss::Area::Save(obs_data_t *obj, const char *name)
{
	auto data = obs_data_create();
	obs_data_set_int(data, "x", x);
	obs_data_set_int(data, "y", y);
	obs_data_set_int(data, "width", width);
	obs_data_set_int(data, "height", height);
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

void advss::Area::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);
	x = obs_data_get_int(data, "x");
	y = obs_data_get_int(data, "y");
	width = obs_data_get_int(data, "width");
	height = obs_data_get_int(data, "height");
	obs_data_release(data);
}

SizeSelection::SizeSelection(int min, int max, QWidget *parent)
	: QWidget(parent), _x(new QSpinBox), _y(new QSpinBox)
{
	_x->setMinimum(min);
	_y->setMinimum(min);
	_x->setMaximum(max);
	_y->setMaximum(max);

	connect(_x, SIGNAL(valueChanged(int)), this, SLOT(XChanged(int)));
	connect(_y, SIGNAL(valueChanged(int)), this, SLOT(YChanged(int)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_x);
	layout->addWidget(new QLabel(" x "));
	layout->addWidget(_y);
	setLayout(layout);
}

void SizeSelection::SetSize(const advss::Size &s)
{
	_x->setValue(s.width);
	_y->setValue(s.height);
}

advss::Size SizeSelection::Size()
{
	return advss::Size{_x->value(), _y->value()};
}

void SizeSelection::XChanged(int value)
{
	emit SizeChanged(advss::Size{value, _y->value()});
}

void SizeSelection::YChanged(int value)
{
	emit SizeChanged(advss::Size{_x->value(), value});
}

AreaSelection::AreaSelection(int min, int max, QWidget *parent)
	: QWidget(parent),
	  _x(new SizeSelection(min, max)),
	  _y(new SizeSelection(min, max))
{
	_x->_x->setToolTip("X");
	_x->_y->setToolTip("Y");
	_y->_x->setToolTip(
		obs_module_text("AdvSceneSwitcher.condition.video.width"));
	_y->_y->setToolTip(
		obs_module_text("AdvSceneSwitcher.condition.video.height"));

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
