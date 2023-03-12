#pragma once
#include <variable-spinbox.hpp>

#include <QWidget>
#include <QSpinBox>
#include <obs-data.h>
#include <opencv2/opencv.hpp>

namespace advss {

struct Size {
	void Save(obs_data_t *obj, const char *name) const;
	void Load(obs_data_t *obj, const char *name);
	cv::Size CV();

	NumberVariable<int> width;
	NumberVariable<int> height;
};

struct Area {
	void Save(obs_data_t *obj, const char *name) const;
	void Load(obs_data_t *obj, const char *name);

	NumberVariable<int> x;
	NumberVariable<int> y;
	NumberVariable<int> width;
	NumberVariable<int> height;
};

}

class SizeSelection : public QWidget {
	Q_OBJECT

public:
	SizeSelection(int min, int max, QWidget *parent = 0);
	void SetSize(const advss::Size &);
	advss::Size Size();

private slots:
	void XChanged(const NumberVariable<int> &);
	void YChanged(const NumberVariable<int> &);
signals:
	void SizeChanged(advss::Size value);

private:
	VariableSpinBox *_x;
	VariableSpinBox *_y;

	friend class AreaSelection;
};

class AreaSelection : public QWidget {
	Q_OBJECT

public:
	AreaSelection(int min, int max, QWidget *parent = 0);
	void SetArea(const advss::Area &);

private slots:
	void XSizeChanged(advss::Size value);
	void YSizeChanged(advss::Size value);
signals:
	void AreaChanged(advss::Area value);

private:
	SizeSelection *_x;
	SizeSelection *_y;
};
