#pragma once
#include <QWidget>

namespace advss {

/*
Simple switch toogle button widget
Based on https://github.com/KDAB/kdabtv/blob/master/Styling-Qt-Widgets/toggleswitch.h
*/
class SwitchButton : public QWidget {
	Q_OBJECT

public:
	explicit SwitchButton(QWidget *parent = nullptr);

	void setChecked(bool checked);
	bool isChecked() const;

	void toggle();

	QSize sizeHint() const override;

signals:
	void checked(bool checked); // by user
	void toggled(bool checked); // by user or by program

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	bool _checked = false;
	bool _mouseDown = false;
};

} // namespace advss
