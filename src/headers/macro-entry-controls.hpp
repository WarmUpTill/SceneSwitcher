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

signals:
	void Add();
	void Remove();
	void Up();
	void Down();

private:
	QPushButton *_add;
	QPushButton *_remove;
	QPushButton *_up;
	QPushButton *_down;
	QPropertyAnimation *_animation = nullptr;
};
