#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>

class MacroSegmentList : public QScrollArea {
	Q_OBJECT

public:
	MacroSegmentList(QWidget *parent = nullptr);
	void SetHelpMsg(const QString &msg);
	void SetHelpMsgVisible(bool visible);
	QVBoxLayout *ContentLayout() { return _contentLayout; }

signals:
	void SelectionChagned(int idx);

protected:
	void mousePressEvent(QMouseEvent *event);

private:
	QVBoxLayout *_contentLayout;
	QLabel *_helpMsg;
};
