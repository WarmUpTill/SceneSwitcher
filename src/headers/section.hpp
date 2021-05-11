#pragma once

#include <QFrame>
#include <QGridLayout>
#include <QParallelAnimationGroup>
#include <QScrollArea>
#include <QToolButton>
#include <QWidget>

class Section : public QWidget {
	Q_OBJECT

public:
	explicit Section(const int animationDuration = 300,
			 QWidget *parent = 0);

	void SetContent(QWidget *w);
	void AddHeaderWidget(QWidget *);

public slots:
	void Collapse(bool collapsed);

private:
	QGridLayout *_mainLayout;
	QHBoxLayout *_headerWidgetLayout;
	QToolButton *_toggleButton;
	QFrame *_headerLine;
	QParallelAnimationGroup *_toggleAnimation = nullptr;
	QScrollArea *_contentArea = nullptr;
	int _animationDuration;
};
