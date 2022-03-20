#pragma once
#include "macro-segment.hpp"

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
	void Insert(int idx, QWidget *widget);
	void Add(QWidget *widget);
	void Remove(int idx);
	void Clear(int idx = 0); // Clear all elements >= idx
	void Highlight(int idx);
	void SetCollapsed(bool);
	void SetSelection(int idx);
	QVBoxLayout *ContentLayout() { return _contentLayout; }

signals:
	void SelectionChagned(int idx);
	void Reorder(int source, int target);

protected:
	bool eventFilter(QObject *object, QEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void dragLeaveEvent(QDragLeaveEvent *event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);

private:
	int GetDragIndex(const QPoint &);
	int GetDropIndex(const QPoint &);
	int GetWidgetIdx(const QPoint &);
	bool IsInListArea(const QPoint &);
	QRect GetContentItemRectWithPadding(int idx);
	void HideLastDropLine();
	MacroSegmentEdit *WidgetAt(int idx);

	int _dragPosition = -1;
	int _dropLineIdx = -1;

	QVBoxLayout *_layout;
	QVBoxLayout *_contentLayout;
	QLabel *_helpMsg;
};
