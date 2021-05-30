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

	void SetContent(QWidget *w, bool collapsed = true);
	void AddHeaderWidget(QWidget *);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
	void AnimationFinished();
	void Collapse(bool collapse);

private:
	void SetupAnimations();
	void CleanUpPreviousContent();

	QGridLayout *_mainLayout;
	QHBoxLayout *_headerWidgetLayout;
	QToolButton *_toggleButton;
	QFrame *_headerLine;
	QParallelAnimationGroup *_toggleAnimation = nullptr;
	QParallelAnimationGroup *_contentAnimation = nullptr;
	QScrollArea *_contentArea = nullptr;
	QWidget *_content = nullptr;
	int _animationDuration;
	std::atomic_bool _transitioning = {false};
	std::atomic_bool _collapsed = {false};
	int _headerHeight = 0;
	int _contentHeight = 0;
};
