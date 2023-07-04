#pragma once

#include <QTimer>
#include <QLabel>
#include <QCheckBox>
#include <QListView>
#include <QStaticText>
#include <QAbstractListModel>
#include <QStyledItemDelegate>

#include <memory>
#include <deque>
#include <chrono>

class QLabel;
class QSpacerItem;
class QHBoxLayout;

// Only used to enable applying "SourceTreeSubItemCheckBox" stylesheet
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

private slots:
	void ExpandClicked(bool checked);
	void EnableHighlight(bool enable);
	void UpdatePaused();
	void HighlightIfExecuted();
	void MacroRenamed(const QString &, const QString &);

private:
	virtual void paintEvent(QPaintEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void Update(bool force);

	enum class Type {
		Unknown,
		Item,
		Group,
		SubItem,
	};

	Type _type = Type::Unknown;

	QSpacerItem *_spacer = nullptr;
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

	friend class MacroTree;
	friend class MacroTreeModel;
};

class MacroTreeModel : public QAbstractListModel {
	Q_OBJECT

public:
	explicit MacroTreeModel(MacroTree *st,
				std::deque<std::shared_ptr<Macro>> &macros);
	~MacroTreeModel() = default;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index,
			      int role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
	virtual Qt::DropActions supportedDropActions() const override;

private:
	void Reset(std::deque<std::shared_ptr<Macro>> &);
	void MoveItemBefore(const std::shared_ptr<Macro> &item,
			    const std::shared_ptr<Macro> &before);
	void MoveItemAfter(const std::shared_ptr<Macro> &item,
			   const std::shared_ptr<Macro> &after);
	void Add(std::shared_ptr<Macro> item);
	void Remove(std::shared_ptr<Macro> item);
	std::shared_ptr<Macro> Neighbor(const std::shared_ptr<Macro> &m,
					bool above) const;
	std::shared_ptr<Macro> FindEndOfGroup(const std::shared_ptr<Macro> &m,
					      bool above) const;
	std::shared_ptr<Macro> GetCurrentMacro() const;
	std::vector<std::shared_ptr<Macro>>
	GetCurrentMacros(const QModelIndexList &) const;
	QString GetNewGroupName();
	void GroupSelectedItems(QModelIndexList &indices);
	void UngroupSelectedGroups(QModelIndexList &indices);
	void ExpandGroup(std::shared_ptr<Macro> item);
	void CollapseGroup(std::shared_ptr<Macro> item);
	void UpdateGroupState(bool update);
	int GetItemMacroIndex(const std::shared_ptr<Macro> &item) const;
	int GetItemModelIndex(const std::shared_ptr<Macro> &item) const;
	bool IsLastItem(std::shared_ptr<Macro> item) const;

	MacroTree *_mt;
	std::deque<std::shared_ptr<Macro>> &_macros;
	bool _hasGroups = false;

	friend class MacroTree;
	friend class MacroTreeItem;
};

class MacroTree : public QListView {
	Q_OBJECT

public:
	explicit MacroTree(QWidget *parent = nullptr);
	void Reset(std::deque<std::shared_ptr<Macro>> &, bool highlight);
	void Add(std::shared_ptr<Macro> item,
		 std::shared_ptr<Macro> after = {}) const;
	void Remove(std::shared_ptr<Macro> item) const;
	void Up(std::shared_ptr<Macro> item) const;
	void Down(std::shared_ptr<Macro> item) const;
	std::shared_ptr<Macro> GetCurrentMacro() const;
	std::vector<std::shared_ptr<Macro>> GetCurrentMacros() const;
	bool GroupsSelected() const;
	bool GroupedItemsSelected() const;
	bool SingleItemSelected() const;
	bool SelectionEmpty() const;

public slots:
	void GroupSelectedItems();
	void UngroupSelectedGroups();

protected:
	virtual void dropEvent(QDropEvent *event) override;
	virtual void paintEvent(QPaintEvent *event) override;

private:
	MacroTreeItem *GetItemWidget(int idx) const;
	void ResetWidgets();
	void UpdateWidget(const QModelIndex &idx, std::shared_ptr<Macro> item);
	void UpdateWidgets(bool force = false);
	void MoveItemBefore(const std::shared_ptr<Macro> &item,
			    const std::shared_ptr<Macro> &after) const;
	void MoveItemAfter(const std::shared_ptr<Macro> &item,
			   const std::shared_ptr<Macro> &after) const;
	MacroTreeModel *GetModel() const;

	bool _highlight = false;

	friend class MacroTreeModel;
	friend class MacroTreeItem;
};

class MacroTreeDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	MacroTreeDelegate(QObject *parent);
	virtual QSize sizeHint(const QStyleOptionViewItem &option,
			       const QModelIndex &index) const override;
};

} // namespace advss
