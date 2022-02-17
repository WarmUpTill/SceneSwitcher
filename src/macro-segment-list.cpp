#include "headers/macro-segment-list.hpp"

#include <QGridLayout>
#include <QSpacerItem>
#include <QMouseEvent>

MacroSegmentList::MacroSegmentList(QWidget *parent) : QScrollArea(parent)
{
	_contentLayout = new QVBoxLayout;
	_helpMsg = new QLabel;
	_helpMsg->setWordWrap(true);
	_helpMsg->setAlignment(Qt::AlignCenter);

	auto helperLayout = new QGridLayout();
	helperLayout->addWidget(_helpMsg, 0, 0,
				Qt::AlignHCenter | Qt::AlignVCenter);
	helperLayout->addLayout(_contentLayout, 0, 0);
	helperLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
	auto layout = new QVBoxLayout;
	layout->addLayout(helperLayout, 10);
	layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
					QSizePolicy::Expanding));
	auto wrapper = new QWidget;
	wrapper->setLayout(layout);
	setWidget(wrapper);
	setWidgetResizable(true);
}

void MacroSegmentList::SetHelpMsg(const QString &msg)
{
	_helpMsg->setText(msg);
}

void MacroSegmentList::SetHelpMsgVisible(bool visible)
{
	_helpMsg->setVisible(visible);
}

void MacroSegmentList::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit SelectionChagned(-1);
	}
}
