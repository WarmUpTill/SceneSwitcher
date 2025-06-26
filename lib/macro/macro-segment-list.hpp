#pragma once
#include "macro-segment.hpp"

#include <thread>

#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace advss {

class Macro;

class MacroSegmentList : public QScrollArea {
	Q_OBJECT

public:
	MacroSegmentList(QWidget *parent = nullptr);
	~MacroSegmentList();

	void SetHelpMsg(const QString &msg) const;
	void SetHelpMsgVisible(bool visible);

	MacroSegmentEdit *WidgetAt(int idx) const;
	MacroSegmentEdit *WidgetAt(const QPoint &) const;

	int IndexAt(const QPoint &) const;
	void Insert(int idx, QWidget *widget);
	void Add(QWidget *widget);
	void Remove(int idx);
	void Clear(int idx = 0); // Clear all elements >= idx
	bool IsEmpty() const;

	static void SetCachingEnabled(bool enable);
	void CacheCurrentWidgetsFor(const Macro *);
	bool PopulateWidgetsFromCache(const Macro *);
	void ClearWidgetsFromCacheFor(const Macro *);
	void SetVisibilityCheckEnable(bool enable);

	void Highlight(int idx, QColor color = QColor(Qt::green));

	void SetCollapsed(bool) const;
	void SetSelection(int idx) const;
	QVBoxLayout *ContentLayout() const { return _contentLayout; }

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

signals:
	void SelectionChanged(int idx);
	void Reorder(int source, int target);

protected:
	bool eventFilter(QObject *object, QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void SetupVisibleMacroSegmentWidgets();
	void ClearWidgetCache();

	int GetSegmentIndexFromPos(const QPoint &) const;
	bool IsInListArea(const QPoint &) const;
	QRect GetContentItemRectWithPadding(int idx) const;

	int GetDragIndex(const QPoint &) const;
	int GetDropIndex(const QPoint &) const;
	void CheckScroll() const;
	void CheckDropLine(const QPoint &);
	void HideLastDropLine();

	int _dragPosition = -1;
	int _dropLineIdx = -1;
	QPoint _dragCursorPos;
	std::thread _autoScrollThread;
	std::atomic_bool _autoScroll{false};

	QStackedWidget *_stackedWidget;
	QVBoxLayout *_layout;
	QVBoxLayout *_contentLayout;
	QLabel *_helpMsg;

	bool _checkVisibility = true;

	static bool _useCache;
	std::unordered_map<const Macro *, std::vector<QWidget *>> _widgetCache;
};

} // namespace advss
