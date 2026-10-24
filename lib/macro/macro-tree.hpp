#pragma once

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeView>

#include <chrono>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

class QLabel;
class QHBoxLayout;

// Only used to enable applying "SourceTreeSubItemCheckBox" stylesheet
// Can be removed once the minimum supported OBS version is greater than 30.1
class SourceTreeSubItemCheckBox : public QCheckBox {
	Q_OBJECT
};

namespace advss {

class Macro;
class MacroTree;

class MacroTreeItem : public QFrame {
	Q_OBJECT

public:
	explicit MacroTreeItem(MacroTree *tree, std::shared_ptr<Macro> macro,
			       bool highlight);
	Macro *GetMacro() const { return _macro.get(); }
	void UpdateExpandState();

protected:
	bool event(QEvent *) override;

public slots:
	void UpdateRunning();

private slots:
	void ExpandClicked(bool checked);
	void EnableHighlight(bool enable);
	void RunningClicked(bool);
	void HighlightIfExecuted();
	void MacroRenamed(const QString &, const QString &);

private:
	virtual void paintEvent(QPaintEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

	QCheckBox *_expand = nullptr;
	QLabel *_iconLabel = nullptr;
	QCheckBox *_running = nullptr;
	QHBoxLayout *_boxLayout = nullptr;
	QLabel *_label = nullptr;
	MacroTree *_tree;
	bool _highlight;
	std::chrono::high_resolution_clock::time_point _lastHighlightCheckTime{};
	QTimer _timer;
	std::shared_ptr<Macro> _macro;
};

// Exposes the flat, pre-ordered list of macros as a tree.
// The structure reported to Qt is only updated from the macro list between the
// begin*() and end*() calls of each modification, so changes of the macro
// parents made before a modification do not confuse the views.
class MacroTreeModel : public QAbstractItemModel {
	Q_OBJECT

public:
	MacroTreeModel(QObject *parent,
		       std::deque<std::shared_ptr<Macro>> &macros);

	QModelIndex index(int row, int column,
			  const QModelIndex &parent = {}) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent = {}) const override;
	int columnCount(const QModelIndex &parent = {}) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	Qt::DropActions supportedDropActions() const override;
	QStringList mimeTypes() const override;
	QMimeData *mimeData(const QModelIndexList &indexes) const override;

	static const char *MimeType();

	QModelIndex IndexOf(const Macro *macro) const;
	std::shared_ptr<Macro> MacroAt(const QModelIndex &index) const;
	Macro *ParentOf(const Macro *macro) const;
	int RowOf(const Macro *macro) const;
	int PositionOf(const Macro *macro) const;
	int ChildCount(const Macro *parent) const;
	Macro *ChildAt(const Macro *parent, int row) const;

	void Insert(const std::shared_ptr<Macro> &item, Macro *parent, int row);
	void Remove(const std::shared_ptr<Macro> &item);
	// row is the position among the children of the new parent before
	// the move via beginMoveRows()
	bool Move(const std::shared_ptr<Macro> &item, Macro *newParent,
		  int row);

private:
	struct Node {
		Macro *parent = nullptr;
		int row = 0;
		size_t position = 0;
	};

	void RebuildCache();
	size_t SubtreeEnd(size_t position) const;
	size_t InsertPosition(const Macro *parent, int row) const;
	std::shared_ptr<Macro> SharedMacro(const Macro *macro) const;

	std::deque<std::shared_ptr<Macro>> &_macros;
	std::unordered_map<const Macro *, Node> _nodes;
	std::unordered_map<const Macro *, std::vector<Macro *>> _children;
};

class MacroTree : public QTreeView {
	Q_OBJECT

public:
	explicit MacroTree(QWidget *parent = nullptr);
	void Reset(std::deque<std::shared_ptr<Macro>> &, bool highlight);
	void Add(std::shared_ptr<Macro> item,
		 std::shared_ptr<Macro> after = {}) const;
	void AddToGroup(std::shared_ptr<Macro> item,
			std::shared_ptr<Macro> group) const;
	void Remove(std::shared_ptr<Macro> item) const;
	void Up(std::shared_ptr<Macro> item) const;
	void Down(std::shared_ptr<Macro> item) const;
	std::shared_ptr<Macro> GetCurrentMacro() const;
	std::vector<std::shared_ptr<Macro>> GetCurrentMacros() const;
	bool GroupsSelected() const;
	bool CanGroupSelection() const;
	bool SingleItemSelected() const;
	bool SelectionEmpty() const;
	bool GroupsExist() const;
	void ExpandGroup(std::shared_ptr<Macro> item);
	void CollapseGroup(std::shared_ptr<Macro> item);
	void SetGroupExpanded(Macro *group, bool expanded);
	void RefreshFilter();
	void UpdateRunningStates();

public slots:
	void GroupSelectedItems();
	void UngroupSelectedGroups();
	void ExpandAll();
	void CollapseAll();
	void SelectionChangedHelper(const QItemSelection &,
				    const QItemSelection &);

signals:
	void MacroSelectionAboutToChange();
	void MacroSelectionChanged();

protected:
	void dropEvent(QDropEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void drawBranches(QPainter *painter, const QRect &rect,
			  const QModelIndex &index) const override;

private slots:
	void SyncExpandState(const QModelIndex &index);
	void UpdateWidgets();

private:
	struct DropTarget {
		Macro *parent = nullptr;
		int row = 0;
	};

	DropTarget DropTargetAt(const QPoint &pos) const;
	bool MoveSelection(const DropTarget &target);
	void UpdateWidgets(const QModelIndex &parent);
	void ApplyExpandState(const QModelIndex &parent);
	void ApplyFilter(const QModelIndex &parent);
	std::vector<std::shared_ptr<Macro>> GetSelectedSubtreeRoots() const;
	QString GetNewGroupName() const;
	MacroTreeModel *GetModel() const;

	bool _highlight = false;
};

class MacroTreeDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	MacroTreeDelegate(QObject *parent);
	QSize sizeHint(const QStyleOptionViewItem &option,
		       const QModelIndex &index) const override;
};

} // namespace advss
