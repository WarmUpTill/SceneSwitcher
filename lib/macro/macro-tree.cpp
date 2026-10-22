#include "macro-tree.hpp"
#include "macro-undo-redo.hpp"

#include "macro-helpers.hpp"
#include "macro-search.hpp"
#include "macro-signals.hpp"
#include "macro.hpp"
#include "path-helpers.hpp"
#include "sync-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs.h>

#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSpacerItem>
#include <QStyleOption>
#include <QToolTip>

#include <algorithm>
#include <functional>
#include <string>

namespace advss {

MacroTreeItem::MacroTreeItem(MacroTree *tree, std::shared_ptr<Macro> macroItem,
			     bool highlight)
	: _running(new QCheckBox()),
	  _boxLayout(new QHBoxLayout()),
	  _label(new QLabel(QString::fromStdString(macroItem->Name()))),
	  _tree(tree),
	  _highlight(highlight),
	  _macro(macroItem)
{
	setAttribute(Qt::WA_TranslucentBackground);
	// This is necessary for some theses to display active selections properly
	setStyleSheet("background: none");

	_running->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_running->setChecked(!_macro->Paused());
	_running->setStyleSheet("background: none");

	_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_label->setAttribute(Qt::WA_TranslucentBackground);

#ifdef __APPLE__
	_running->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif

	_boxLayout->setContentsMargins(0, 0, 0, 0);
	if (_macro->IsGroup()) {
		_expand = new SourceTreeSubItemCheckBox();
		_expand->setProperty("sourceTreeSubItem", true);
		_expand->setSizePolicy(QSizePolicy::Maximum,
				       QSizePolicy::Maximum);
		_expand->setMaximumSize(10, 16);
		_expand->setMinimumSize(10, 0);
		_expand->setProperty("class", "checkbox-icon indicator-expand");
#ifdef __APPLE__
		_expand->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif
		_expand->setChecked(_macro->IsCollapsed());
		connect(_expand, &QCheckBox::toggled, this,
			&MacroTreeItem::ExpandClicked);
		_boxLayout->addWidget(_expand);

		const auto path = QString::fromStdString(GetDataFilePath(
			"res/images/" + GetThemeTypeName() + "Group.svg"));
		QIcon icon(path);
		QPixmap pixmap = icon.pixmap(QSize(16, 16));
		_iconLabel = new QLabel();
		_iconLabel->setPixmap(pixmap);
		_iconLabel->setStyleSheet("background: none");
		_boxLayout->addWidget(_iconLabel);
		_boxLayout->addSpacing(2);
	} else {
		_boxLayout->addSpacing(3);
	}
	_boxLayout->addWidget(_running);
	_boxLayout->addWidget(_label);
#ifdef __APPLE__
	/* Hack: Fixes a bug where scrollbars would be above the lock icon */
	_boxLayout->addSpacing(16);
#endif
	setLayout(_boxLayout);

	connect(_running, SIGNAL(clicked(bool)), this,
		SLOT(RunningClicked(bool)));
	connect(MacroSignalManager::Instance(), SIGNAL(HighlightChanged(bool)),
		this, SLOT(EnableHighlight(bool)));
	connect(MacroSignalManager::Instance(),
		SIGNAL(Rename(const QString &, const QString &)), this,
		SLOT(MacroRenamed(const QString &, const QString &)));
	connect(&_timer, SIGNAL(timeout()), this, SLOT(HighlightIfExecuted()));
	connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdateRunning()));

	UpdateRunning();
	_timer.start(1500);
}

void MacroTreeItem::UpdateExpandState()
{
	if (!_expand) {
		return;
	}
	const QSignalBlocker blocker(_expand);
	_expand->setChecked(_macro->IsCollapsed());
}

bool MacroTreeItem::event(QEvent *event)
{
	if (event->type() != QEvent::ToolTip) {
		return QFrame::event(event);
	}

	if (_macro->IsGroup()) {
		return true;
	}

	QString text;
	if (!_macro->WasExecutedSince({})) {
		text = obs_module_text(
			"AdvSceneSwitcher.macroTab.macroNotYetExecutedTooltip");
	} else {
		const QString formatStr = obs_module_text(
			"AdvSceneSwitcher.macroTab.macroLastExecutedTooltip");
		const auto secondsSinceLastRun =
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::high_resolution_clock::now() -
				_macro->GetLastExecutionTime());
		text = formatStr.arg(secondsSinceLastRun.count());
	}

	auto helpEvent = static_cast<QHelpEvent *>(event);
	QToolTip::showText(helpEvent->globalPos(), text, this);
	return true;
}

void MacroTreeItem::EnableHighlight(bool enable)
{
	_highlight = enable;
}

void MacroTreeItem::RunningClicked(bool running)
{
	if (_macro->IsGroup()) {
		for (const auto &macro : GetGroupMacroEntries(_macro.get())) {
			macro->SetPaused(!running);
		}
	} else {
		_macro->SetPaused(!running);
	}

	// Update backend values before updating UI to prevent flickering in
	// running state of the group
	_tree->UpdateRunningStates();
}

void MacroTreeItem::UpdateRunning()
{
	const QSignalBlocker blocker(_running);

	if (!_macro->IsGroup()) {
		_running->setChecked(!_macro->Paused());
		return;
	}

	const auto macros = GetGroupMacroEntries(_macro.get());
	bool allRunning = true;
	bool allPaused = true;
	for (const auto &macro : macros) {
		if (macro->Paused()) {
			allRunning = false;
		} else {
			allPaused = false;
		}
	}

	if (allRunning) {
		_running->setCheckState(Qt::Checked);
		return;
	}

	if (allPaused) {
		_running->setCheckState(Qt::Unchecked);
		return;
	}

	_running->setCheckState(Qt::PartiallyChecked);
}

void MacroTreeItem::HighlightIfExecuted()
{
	if (!_highlight || !_macro) {
		return;
	}

	bool wasHighlighted = false;
	if (_lastHighlightCheckTime.time_since_epoch().count() != 0 &&
	    _macro->WasExecutedSince(_lastHighlightCheckTime)) {
		HighlightWidget(this, Qt::green, QColor(0, 0, 0, 0), true);
		wasHighlighted = true;
	}

	if (!wasHighlighted &&
	    _lastHighlightCheckTime.time_since_epoch().count() != 0 &&
	    _macro->ActionTriggerModePreventedActionsSince(
		    _lastHighlightCheckTime)) {
		HighlightWidget(this, Qt::yellow, QColor(0, 0, 0, 0), true);
	}

	_lastHighlightCheckTime = std::chrono::high_resolution_clock::now();
}

void MacroTreeItem::MacroRenamed(const QString &oldName, const QString &newName)
{
	if (_label->text() == oldName) {
		_label->setText(newName);
	}
}

void MacroTreeItem::paintEvent(QPaintEvent *event)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

	QWidget::paintEvent(event);
}

void MacroTreeItem::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget::mouseDoubleClickEvent(event);
	if (_expand) {
		_expand->setChecked(!_expand->isChecked());
	}
}

void MacroTreeItem::ExpandClicked(bool checked)
{
	_tree->SetGroupExpanded(_macro.get(), !checked);
}

/* ========================================================================= */

MacroTreeModel::MacroTreeModel(QObject *parent,
			       std::deque<std::shared_ptr<Macro>> &macros)
	: QAbstractItemModel(parent),
	  _macros(macros)
{
	RebuildCache();
}

const char *MacroTreeModel::MimeType()
{
	return "application/x-advss-macro-tree";
}

void MacroTreeModel::RebuildCache()
{
	_nodes.clear();
	_children.clear();
	_children[nullptr];

	for (size_t i = 0; i < _macros.size(); i++) {
		auto macro = _macros[i].get();
		auto parent = macro->Parent().get();
		if (parent && _nodes.find(parent) == _nodes.end()) {
			blog(LOG_WARNING,
			     "parent of macro \"%s\" not found in macro tree",
			     macro->Name().c_str());
			parent = nullptr;
		}
		auto &siblings = _children[parent];
		_nodes[macro] = {parent, (int)siblings.size(), i};
		siblings.emplace_back(macro);
	}
}

static bool isCachedDescendant(
	const std::unordered_map<const Macro *, std::vector<Macro *>> &children,
	const Macro *macro, const Macro *ancestor)
{
	auto it = children.find(ancestor);
	if (it == children.end()) {
		return false;
	}
	for (const auto child : it->second) {
		if (child == macro ||
		    isCachedDescendant(children, macro, child)) {
			return true;
		}
	}
	return false;
}

size_t MacroTreeModel::SubtreeEnd(size_t position) const
{
	const auto root = _macros[position].get();
	size_t end = position + 1;
	while (end < _macros.size() &&
	       isCachedDescendant(_children, _macros[end].get(), root)) {
		end++;
	}
	return end;
}

size_t MacroTreeModel::InsertPosition(const Macro *parent, int row) const
{
	if (row < ChildCount(parent)) {
		return _nodes.at(ChildAt(parent, row)).position;
	}
	if (parent) {
		return SubtreeEnd(_nodes.at(parent).position);
	}
	return _macros.size();
}

std::shared_ptr<Macro> MacroTreeModel::SharedMacro(const Macro *macro) const
{
	auto it = _nodes.find(macro);
	if (it == _nodes.end()) {
		return {};
	}
	return _macros[it->second.position];
}

QModelIndex MacroTreeModel::index(int row, int column,
				  const QModelIndex &parent) const
{
	if (column != 0 || row < 0) {
		return {};
	}
	auto parentMacro = static_cast<const Macro *>(parent.internalPointer());
	auto it = _children.find(parent.isValid() ? parentMacro : nullptr);
	if (it == _children.end() || row >= (int)it->second.size()) {
		return {};
	}
	return createIndex(row, 0, it->second[row]);
}

QModelIndex MacroTreeModel::parent(const QModelIndex &index) const
{
	if (!index.isValid()) {
		return {};
	}
	auto it = _nodes.find(
		static_cast<const Macro *>(index.internalPointer()));
	if (it == _nodes.end() || !it->second.parent) {
		return {};
	}
	return IndexOf(it->second.parent);
}

int MacroTreeModel::rowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0) {
		return 0;
	}
	return ChildCount(parent.isValid() ? static_cast<const Macro *>(
						     parent.internalPointer())
					   : nullptr);
}

int MacroTreeModel::columnCount(const QModelIndex &) const
{
	return 1;
}

QVariant MacroTreeModel::data(const QModelIndex &index, int role) const
{
	if (role != Qt::AccessibleTextRole) {
		return {};
	}
	auto macro = MacroAt(index);
	if (!macro) {
		return {};
	}
	return QString::fromStdString(macro->Name());
}

Qt::ItemFlags MacroTreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid()) {
		return Qt::ItemIsDropEnabled;
	}

	auto macro = MacroAt(index);
	if (!macro) {
		return Qt::NoItemFlags;
	}
	const Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled |
				    Qt::ItemIsDragEnabled;
	return macro->IsGroup() ? flags | Qt::ItemIsDropEnabled : flags;
}

Qt::DropActions MacroTreeModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList MacroTreeModel::mimeTypes() const
{
	return {MimeType()};
}

QMimeData *MacroTreeModel::mimeData(const QModelIndexList &indexes) const
{
	QStringList names;
	for (const auto &index : indexes) {
		names << index.data(Qt::AccessibleTextRole).toString();
	}
	auto data = new QMimeData();
	data->setData(MimeType(), names.join('\n').toUtf8());
	return data;
}

QModelIndex MacroTreeModel::IndexOf(const Macro *macro) const
{
	auto it = _nodes.find(macro);
	if (it == _nodes.end()) {
		return {};
	}
	return createIndex(it->second.row, 0, const_cast<Macro *>(macro));
}

std::shared_ptr<Macro> MacroTreeModel::MacroAt(const QModelIndex &index) const
{
	if (!index.isValid() || index.model() != this) {
		return {};
	}
	return SharedMacro(static_cast<const Macro *>(index.internalPointer()));
}

Macro *MacroTreeModel::ParentOf(const Macro *macro) const
{
	auto it = _nodes.find(macro);
	return it == _nodes.end() ? nullptr : it->second.parent;
}

int MacroTreeModel::RowOf(const Macro *macro) const
{
	auto it = _nodes.find(macro);
	return it == _nodes.end() ? -1 : it->second.row;
}

int MacroTreeModel::PositionOf(const Macro *macro) const
{
	auto it = _nodes.find(macro);
	return it == _nodes.end() ? -1 : (int)it->second.position;
}

int MacroTreeModel::ChildCount(const Macro *parent) const
{
	auto it = _children.find(parent);
	return it == _children.end() ? 0 : (int)it->second.size();
}

Macro *MacroTreeModel::ChildAt(const Macro *parent, int row) const
{
	auto it = _children.find(parent);
	if (it == _children.end() || row < 0 || row >= (int)it->second.size()) {
		return nullptr;
	}
	return it->second[row];
}

void MacroTreeModel::Insert(const std::shared_ptr<Macro> &item, Macro *parent,
			    int row)
{
	if (!item || _nodes.find(item.get()) != _nodes.end() ||
	    (parent && _nodes.find(parent) == _nodes.end())) {
		return;
	}

	row = std::clamp(row, 0, ChildCount(parent));
	const auto position = InsertPosition(parent, row);
	const auto parentMacro = SharedMacro(parent);

	beginInsertRows(IndexOf(parent), row, row);
	Macro::PrepareMoveToGroup(parentMacro, item);
	_macros.insert(_macros.begin() + position, item);
	RebuildCache();
	endInsertRows();
}

void MacroTreeModel::Remove(const std::shared_ptr<Macro> &item)
{
	auto it = _nodes.find(item.get());
	if (it == _nodes.end()) {
		return;
	}

	const auto node = it->second;
	const auto end = SubtreeEnd(node.position);

	beginRemoveRows(IndexOf(node.parent), node.row, node.row);
	Macro::PrepareMoveToGroup(nullptr, item);
	_macros.erase(_macros.begin() + node.position, _macros.begin() + end);
	RebuildCache();
	endRemoveRows();
}

bool MacroTreeModel::Move(const std::shared_ptr<Macro> &item, Macro *newParent,
			  int row)
{
	auto it = _nodes.find(item.get());
	if (it == _nodes.end() ||
	    (newParent && _nodes.find(newParent) == _nodes.end())) {
		return false;
	}

	const auto node = it->second;
	if (newParent == item.get() ||
	    isCachedDescendant(_children, newParent, item.get())) {
		return false;
	}

	row = std::clamp(row, 0, ChildCount(newParent));
	if (node.parent == newParent &&
	    (row == node.row || row == node.row + 1)) {
		return false;
	}

	const auto insertBefore = ChildAt(newParent, row);
	const auto newParentMacro = SharedMacro(newParent);
	if (!beginMoveRows(IndexOf(node.parent), node.row, node.row,
			   IndexOf(newParent), row)) {
		return false;
	}

	const auto start = _macros.begin() + node.position;
	const auto end = _macros.begin() + SubtreeEnd(node.position);
	const std::vector<std::shared_ptr<Macro>> subtree(start, end);
	_macros.erase(start, end);

	auto position = _macros.end();
	if (insertBefore) {
		position = std::find_if(
			_macros.begin(), _macros.end(),
			[insertBefore](const std::shared_ptr<Macro> &macro) {
				return macro.get() == insertBefore;
			});
	} else if (newParent) {
		position = std::find(_macros.begin(), _macros.end(),
				     newParentMacro);
		position = std::find_if(
			std::next(position), _macros.end(),
			[this, newParent](const std::shared_ptr<Macro> &m) {
				return !isCachedDescendant(_children, m.get(),
							   newParent);
			});
	}
	_macros.insert(position, subtree.begin(), subtree.end());

	Macro::PrepareMoveToGroup(newParentMacro, item);
	RebuildCache();
	endMoveRows();
	return true;
}

/* ========================================================================= */

MacroTree::MacroTree(QWidget *parent) : QTreeView(parent)
{
	setStyleSheet(QString(
		"*[bgColor=\"1\"]{background-color:rgba(255,68,68,33%);}"
		"*[bgColor=\"2\"]{background-color:rgba(255,255,68,33%);}"
		"*[bgColor=\"3\"]{background-color:rgba(68,255,68,33%);}"
		"*[bgColor=\"4\"]{background-color:rgba(68,255,255,33%);}"
		"*[bgColor=\"5\"]{background-color:rgba(68,68,255,33%);}"
		"*[bgColor=\"6\"]{background-color:rgba(255,68,255,33%);}"
		"*[bgColor=\"7\"]{background-color:rgba(68,68,68,33%);}"
		"*[bgColor=\"8\"]{background-color:rgba(255,255,255,33%);}"));

	setHeaderHidden(true);
	setRootIsDecorated(false);
	setIndentation(16);
	setExpandsOnDoubleClick(false);
	// Groups are expanded and collapsed using the checkbox of the group
	setItemsExpandable(false);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setItemDelegate(new MacroTreeDelegate(this));

	connect(this, &QTreeView::expanded, this, &MacroTree::SyncExpandState);
	connect(this, &QTreeView::collapsed, this, &MacroTree::SyncExpandState);
}

void MacroTree::Reset(std::deque<std::shared_ptr<Macro>> &macros,
		      bool highlight)
{
	_highlight = highlight;

	auto oldModel = model();
	auto oldSelectionModel = selectionModel();
	auto newModel = new MacroTreeModel(this, macros);
	setModel(newModel);
	if (oldSelectionModel) {
		oldSelectionModel->deleteLater();
	}
	if (oldModel) {
		oldModel->deleteLater();
	}

	connect(newModel, &QAbstractItemModel::rowsInserted, this,
		qOverload<>(&MacroTree::UpdateWidgets));
	connect(newModel, &QAbstractItemModel::rowsMoved, this,
		qOverload<>(&MacroTree::UpdateWidgets));
	connect(newModel, &QAbstractItemModel::rowsRemoved, this,
		qOverload<>(&MacroTree::UpdateWidgets));
	connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
		&MacroTree::SelectionChangedHelper);

	UpdateWidgets();
}

void MacroTree::UpdateWidgets()
{
	UpdateWidgets(QModelIndex());
	ApplyExpandState(QModelIndex());
	ApplyFilter(QModelIndex());
}

void MacroTree::UpdateWidgets(const QModelIndex &parent)
{
	auto treeModel = GetModel();
	for (int row = 0; row < treeModel->rowCount(parent); row++) {
		const auto index = treeModel->index(row, 0, parent);
		const auto macro = treeModel->MacroAt(index);
		auto widget = qobject_cast<MacroTreeItem *>(indexWidget(index));
		if (!widget || widget->GetMacro() != macro.get()) {
			setIndexWidget(index, new MacroTreeItem(this, macro,
								_highlight));
		}
		UpdateWidgets(index);
	}
}

void MacroTree::ApplyExpandState(const QModelIndex &parent)
{
	auto treeModel = GetModel();
	for (int row = 0; row < treeModel->rowCount(parent); row++) {
		const auto index = treeModel->index(row, 0, parent);
		const auto macro = treeModel->MacroAt(index);
		if (!macro->IsGroup()) {
			continue;
		}
		const bool expand = !macro->IsCollapsed();
		if (isExpanded(index) != expand) {
			setExpanded(index, expand);
		}
		ApplyExpandState(index);
	}
}

void MacroTree::ApplyFilter(const QModelIndex &parent)
{
	auto treeModel = GetModel();
	for (int row = 0; row < treeModel->rowCount(parent); row++) {
		const auto index = treeModel->index(row, 0, parent);
		const auto macro = treeModel->MacroAt(index);
		setRowHidden(row, parent,
			     !MacroMatchesSearchFilter(macro.get()));
		ApplyFilter(index);
	}
}

void MacroTree::SyncExpandState(const QModelIndex &index)
{
	const auto macro = GetModel()->MacroAt(index);
	if (!macro || !macro->IsGroup()) {
		return;
	}

	const bool collapsed = !isExpanded(index);
	macro->SetCollapsed(collapsed);
	if (auto widget = qobject_cast<MacroTreeItem *>(indexWidget(index))) {
		widget->UpdateExpandState();
	}

	if (!collapsed) {
		return;
	}

	// Avoid hidden macros being modified by actions on the selection
	for (const auto &selected : selectionModel()->selectedIndexes()) {
		const auto selectedMacro = GetModel()->MacroAt(selected);
		if (IsMacroInGroup(selectedMacro.get(), macro.get())) {
			selectionModel()->select(selected,
						 QItemSelectionModel::Deselect);
		}
	}
}

void MacroTree::SetGroupExpanded(Macro *group, bool expanded)
{
	if (!group || !group->IsGroup()) {
		return;
	}

	group->SetCollapsed(!expanded);
	const auto index = GetModel()->IndexOf(group);
	if (!index.isValid()) {
		return;
	}
	setExpanded(index, expanded);
	SyncExpandState(index);
}

void MacroTree::Add(std::shared_ptr<Macro> item,
		    std::shared_ptr<Macro> after) const
{
	auto treeModel = GetModel();
	{
		auto lock = LockContext();
		Macro *parent = nullptr;
		int row = treeModel->ChildCount(nullptr);
		if (after && treeModel->RowOf(after.get()) != -1) {
			parent = treeModel->ParentOf(after.get());
			row = treeModel->RowOf(after.get()) + 1;
		}
		treeModel->Insert(item, parent, row);
	}
	selectionModel()->setCurrentIndex(treeModel->IndexOf(item.get()),
					  QItemSelectionModel::ClearAndSelect);
}

void MacroTree::AddToGroup(std::shared_ptr<Macro> item,
			   std::shared_ptr<Macro> group) const
{
	auto treeModel = GetModel();
	{
		auto lock = LockContext();
		treeModel->Insert(item, group.get(), 0);
	}
	selectionModel()->setCurrentIndex(treeModel->IndexOf(item.get()),
					  QItemSelectionModel::ClearAndSelect);
}

void MacroTree::Remove(std::shared_ptr<Macro> item) const
{
	{
		auto lock = LockContext();
		GetModel()->Remove(item);
	}
	selectionModel()->clear();
}

void MacroTree::Up(std::shared_ptr<Macro> item) const
{
	auto lock = LockContext();
	auto treeModel = GetModel();
	const int row = treeModel->RowOf(item.get());
	if (row <= 0) {
		return;
	}
	treeModel->Move(item, treeModel->ParentOf(item.get()), row - 1);
}

void MacroTree::Down(std::shared_ptr<Macro> item) const
{
	auto lock = LockContext();
	auto treeModel = GetModel();
	const int row = treeModel->RowOf(item.get());
	const auto parent = treeModel->ParentOf(item.get());
	if (row == -1 || row + 1 >= treeModel->ChildCount(parent)) {
		return;
	}
	treeModel->Move(item, parent, row + 2);
}

std::shared_ptr<Macro> MacroTree::GetCurrentMacro() const
{
	const auto selection = selectionModel()->selectedIndexes();
	if (selection.empty()) {
		return {};
	}
	return GetModel()->MacroAt(selection.back());
}

std::vector<std::shared_ptr<Macro>> MacroTree::GetCurrentMacros() const
{
	std::vector<std::shared_ptr<Macro>> result;
	for (const auto &index : selectionModel()->selectedIndexes()) {
		auto macro = GetModel()->MacroAt(index);
		if (macro) {
			result.emplace_back(macro);
		}
	}
	return result;
}

std::vector<std::shared_ptr<Macro>> MacroTree::GetSelectedSubtreeRoots() const
{
	auto selected = GetCurrentMacros();
	std::vector<std::shared_ptr<Macro>> roots;
	for (const auto &macro : selected) {
		const bool hasSelectedAncestor = std::any_of(
			selected.begin(), selected.end(),
			[&macro](const std::shared_ptr<Macro> &other) {
				return IsMacroInGroup(macro.get(), other.get());
			});
		if (!hasSelectedAncestor) {
			roots.emplace_back(macro);
		}
	}

	auto treeModel = GetModel();
	std::sort(roots.begin(), roots.end(),
		  [treeModel](const std::shared_ptr<Macro> &a,
			      const std::shared_ptr<Macro> &b) {
			  return treeModel->PositionOf(a.get()) <
				 treeModel->PositionOf(b.get());
		  });
	return roots;
}

bool MacroTree::GroupsSelected() const
{
	const auto selected = GetCurrentMacros();
	return std::any_of(selected.begin(), selected.end(),
			   [](const std::shared_ptr<Macro> &macro) {
				   return macro->IsGroup();
			   });
}

bool MacroTree::CanGroupSelection() const
{
	const auto items = GetSelectedSubtreeRoots();
	if (items.empty()) {
		return false;
	}

	auto treeModel = GetModel();
	const auto parent = treeModel->ParentOf(items.front().get());
	return std::all_of(
		items.begin(), items.end(),
		[treeModel, parent](const std::shared_ptr<Macro> &m) {
			return treeModel->ParentOf(m.get()) == parent;
		});
}

bool MacroTree::SingleItemSelected() const
{
	return selectionModel()->selectedIndexes().size() == 1;
}

bool MacroTree::SelectionEmpty() const
{
	return selectionModel()->selectedIndexes().empty();
}

bool MacroTree::GroupsExist() const
{
	const auto &macros = GetTopLevelMacros();
	return std::any_of(macros.begin(), macros.end(),
			   [](const std::shared_ptr<Macro> &macro) {
				   return macro->IsGroup();
			   });
}

void MacroTree::ExpandGroup(std::shared_ptr<Macro> item)
{
	SetGroupExpanded(item.get(), true);
}

void MacroTree::CollapseGroup(std::shared_ptr<Macro> item)
{
	SetGroupExpanded(item.get(), false);
}

void MacroTree::RefreshFilter()
{
	ApplyFilter(QModelIndex());
}

void MacroTree::UpdateRunningStates()
{
	std::function<void(const QModelIndex &)> update =
		[this, &update](const QModelIndex &parent) {
			for (int row = 0; row < model()->rowCount(parent);
			     row++) {
				const auto index =
					model()->index(row, 0, parent);
				if (auto widget = qobject_cast<MacroTreeItem *>(
					    indexWidget(index))) {
					widget->UpdateRunning();
				}
				update(index);
			}
		};
	update(QModelIndex());
}

MacroTree::DropTarget MacroTree::DropTargetAt(const QPoint &pos) const
{
	auto treeModel = GetModel();
	const auto index = indexAt(pos);
	if (!index.isValid()) {
		return {nullptr, treeModel->ChildCount(nullptr)};
	}

	const auto macro = treeModel->MacroAt(index).get();
	const auto parent = treeModel->ParentOf(macro);
	const auto rect = visualRect(index);

	if (macro->IsGroup()) {
		// Same margin QAbstractItemView uses for the drop indicator
		const int margin = std::clamp(int(rect.height() / 5.5), 2, 12);
		const bool onItem = pos.y() - rect.top() >= margin &&
				    rect.bottom() - pos.y() >= margin;
		const bool belowExpanded = pos.y() > rect.center().y() &&
					   isExpanded(index);
		if (onItem || belowExpanded) {
			return {macro, 0};
		}
	}

	if (pos.y() < rect.center().y()) {
		return {parent, index.row()};
	}
	return {parent, index.row() + 1};
}

bool MacroTree::MoveSelection(const DropTarget &target)
{
	const auto items = GetSelectedSubtreeRoots();
	if (items.empty()) {
		return false;
	}

	for (const auto &item : items) {
		if (target.parent == item.get() ||
		    IsMacroInGroup(target.parent, item.get())) {
			return false;
		}
	}

	auto lock = LockContext();
	auto treeModel = GetModel();
	const auto insertBefore = treeModel->ChildAt(target.parent, target.row);
	bool moved = false;
	for (const auto &item : items) {
		const int row = insertBefore
					? treeModel->RowOf(insertBefore)
					: treeModel->ChildCount(target.parent);
		moved |= treeModel->Move(item, target.parent, row);
	}
	return moved;
}

void MacroTree::dropEvent(QDropEvent *event)
{
	if (event->source() != this ||
	    !event->mimeData()->hasFormat(MacroTreeModel::MimeType())) {
		QTreeView::dropEvent(event);
		return;
	}

	MoveSelection(DropTargetAt(event->position().toPoint()));

	// Prevent QAbstractItemView::startDrag() from removing the dragged
	// rows, as they were already moved
	event->setDropAction(Qt::CopyAction);
	event->accept();
	stopAutoScroll();
	setState(NoState);
	viewport()->update();
}

QString MacroTree::GetNewGroupName() const
{
	QString fmt =
		obs_module_text("AdvSceneSwitcher.macroTab.defaultGroupName");
	QString name = fmt.arg("1");

	int i = 2;
	for (;;) {
		if (!GetMacroByQString(name)) {
			break;
		}
		name = fmt.arg(QString::number(i++));
	}

	return name;
}

void MacroTree::GroupSelectedItems()
{
	if (!CanGroupSelection()) {
		return;
	}

	const auto items = GetSelectedSubtreeRoots();
	auto treeModel = GetModel();
	const auto parent = treeModel->ParentOf(items.front().get());
	const auto name = GetNewGroupName().toStdString();
	std::shared_ptr<Macro> group;
	{
		auto lock = LockContext();
		std::vector<std::shared_ptr<Macro>> noChildren;
		group = Macro::CreateGroup(name, noChildren);
		treeModel->Insert(group, parent,
				  treeModel->RowOf(items.front().get()));
		int row = 0;
		for (const auto &item : items) {
			treeModel->Move(item, group.get(), row++);
		}
		RegisterGroupCreateUndoRedo(name);
	}

	selectionModel()->clear();
	SetGroupExpanded(group.get(), true);
}

void MacroTree::UngroupSelectedGroups()
{
	std::vector<std::shared_ptr<Macro>> groups;
	for (const auto &macro : GetCurrentMacros()) {
		if (macro->IsGroup()) {
			groups.emplace_back(macro);
		}
	}
	if (groups.empty()) {
		return;
	}

	{
		auto lock = LockContext();
		auto treeModel = GetModel();
		for (const auto &group : groups) {
			RegisterGroupRemoveUndoRedo(group->Name());
			const auto parent = treeModel->ParentOf(group.get());
			const int row = treeModel->RowOf(group.get());
			const auto children = GetGroupChildren(group.get());
			int offset = 1;
			for (const auto &child : children) {
				treeModel->Move(child, parent, row + offset++);
			}
			treeModel->Remove(group);
		}
	}

	selectionModel()->clear();
}

void MacroTree::ExpandAll()
{
	for (const auto &macro : GetTopLevelMacros()) {
		SetGroupExpanded(macro.get(), true);
	}
}

void MacroTree::CollapseAll()
{
	for (const auto &macro : GetTopLevelMacros()) {
		SetGroupExpanded(macro.get(), false);
	}
}

void MacroTree::SelectionChangedHelper(const QItemSelection &,
				       const QItemSelection &)
{
	emit MacroSelectionAboutToChange();
	emit MacroSelectionChanged();
}

void MacroTree::paintEvent(QPaintEvent *event)
{
	if (model() && model()->rowCount() == 0) {
		QPainter painter(viewport());
		const QRect rectangle =
			QRect(0, 0, size().width(), size().height());
		QRect boundingRect;
		QString text(obs_module_text("AdvSceneSwitcher.macroTab.help"));
		painter.drawText(rectangle, Qt::AlignCenter | Qt::TextWordWrap,
				 text, &boundingRect);
	} else {
		QTreeView::paintEvent(event);
	}
}

void MacroTree::drawBranches(QPainter *painter, const QRect &rect,
			     const QModelIndex &index) const
{
	// Only draw the background, as the branch indicators would duplicate
	// the expand checkbox of nested groups
	QStyleOptionViewItem option;
	initViewItemOption(&option);
	option.rect = rect;
	if (selectionModel()->isSelected(index)) {
		option.state |= QStyle::State_Selected;
	}
	style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &option, painter,
			       this);
}

MacroTreeModel *MacroTree::GetModel() const
{
	return qobject_cast<MacroTreeModel *>(model());
}

MacroTreeDelegate::MacroTreeDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

QSize MacroTreeDelegate::sizeHint(const QStyleOptionViewItem &option,
				  const QModelIndex &index) const
{
	auto tree = qobject_cast<MacroTree *>(parent());
	auto widget = tree ? tree->indexWidget(index) : nullptr;
	if (!widget) {
		return QStyledItemDelegate::sizeHint(option, index);
	}
	return widget->sizeHint();
}

} // namespace advss
