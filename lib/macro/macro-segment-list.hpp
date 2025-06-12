#pragma once
#include "macro-segment.hpp"

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <thread>

namespace advss {

class Macro;

class MacroSegmentList : public QScrollArea {
	Q_OBJECT

public:
	MacroSegmentList(QWidget *parent = nullptr);
	~MacroSegmentList();
	void SetHelpMsg(const QString &msg) const;
	void SetHelpMsgVisible(bool visible) const;
	MacroSegmentEdit *WidgetAt(int idx) const;
	MacroSegmentEdit *WidgetAt(const QPoint &) const;
	int IndexAt(const QPoint &) const;
	void Insert(int idx, QWidget *widget);
	void Add(QWidget *widget);
	void Remove(int idx) const;
	void Clear(int idx = 0) const; // Clear all elements >= idx
	static void SetCachingEnabled(bool enable);
	void CacheCurrentWidgetsFor(const Macro *);
	bool PopulateWidgetsFromCache(const Macro *);
	void ClearWidgetsFromCacheFor(const Macro *);
	void Highlight(int idx, QColor color = QColor(Qt::green));
	void SetCollapsed(bool) const;
	void SetSelection(int idx) const;
	QVBoxLayout *ContentLayout() const { return _contentLayout; }
	void SetVisibilityCheckEnable(bool enable);

signals:
	void SelectionChanged(int idx);
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
	void resizeEvent(QResizeEvent *event);

private:
	void SetupVisibleMacroSegmentWidgets();
	int GetSegmentIndexFromPos(const QPoint &) const;
	int GetDragIndex(const QPoint &) const;
	int GetDropIndex(const QPoint &) const;
	void CheckScroll() const;
	void CheckDropLine(const QPoint &);
	bool IsInListArea(const QPoint &) const;
	QRect GetContentItemRectWithPadding(int idx) const;
	void HideLastDropLine();
	void ClearWidgetCache();

	int _dragPosition = -1;
	int _dropLineIdx = -1;
	QPoint _dragCursorPos;
	std::thread _autoScrollThread;
	std::atomic_bool _autoScroll{false};

	QVBoxLayout *_layout;
	QVBoxLayout *_contentLayout;
	QLabel *_helpMsg;

	bool _checkVisibility = true;

	static bool _useCache;
	std::unordered_map<const Macro *, std::vector<QWidget *>> _widgetCache;
};

} // namespace advss
