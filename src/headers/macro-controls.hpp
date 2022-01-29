#pragma once

#include <QFrame>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QPushButton>
#include <QLayout>
#include <QWidget>

class MacroEntryControls : public QWidget {
	Q_OBJECT

public:
	explicit MacroEntryControls(const int animationDuration = 300,
				    QWidget *parent = 0);

	void Show(bool visible = true);

	QPushButton *GetUp() { return _up; }
	QPushButton *GetDown() { return _down; }
	QPushButton *GetAdd() { return _add; }
	QPushButton *GetRemove() { return _remove; }

signals:
	void Add();
	void Remove();
	void Up();
	void Down();

private:
	bool _visible = false;
	QPushButton *_add;
	QPushButton *_remove;
	QPushButton *_up;
	QPushButton *_down;
	QPropertyAnimation *_animation = nullptr;
};
