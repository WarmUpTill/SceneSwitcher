#include "macro-tree.hpp"
#include "macro.hpp"
#include "path-helpers.hpp"
#include "sync-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <obs.h>
#include <string>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStylePainter>

Q_DECLARE_METATYPE(std::shared_ptr<advss::Macro>);

namespace advss {

MacroTreeItem::MacroTreeItem(MacroTree *tree, std::shared_ptr<Macro> macroItem,
			     bool highlight)
	: _tree(tree),
	  _highlight(highlight),
	  _macro(macroItem)
{
	setAttribute(Qt::WA_TranslucentBackground);
	// This is necessary for some theses to display active selections properly
	setStyleSheet("background: none");

	auto name = _macro->Name();
	bool macroPaused = _macro->Paused();
	bool isGroup = _macro->IsGroup();

	if (isGroup) {
		const auto path = QString::fromStdString(GetDataFilePath(
			"res/images/" + GetThemeTypeName() + "Group.svg"));
		QIcon icon(path);
		QPixmap pixmap = icon.pixmap(QSize(16, 16));
		_iconLabel = new QLabel();
		_iconLabel->setPixmap(pixmap);
		_iconLabel->setStyleSheet("background: none");
	}

	_running = new QCheckBox();
	_running->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	_running->setChecked(!macroPaused);
	_running->setStyleSheet("background: none");

	_label = new QLabel(QString::fromStdString(name));
	_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	_label->setAttribute(Qt::WA_TranslucentBackground);

#ifdef __APPLE__
	_running->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif

	_boxLayout = new QHBoxLayout();
	_boxLayout->setContentsMargins(0, 0, 0, 0);
	_boxLayout->addWidget(_running);
	if (isGroup) {
		_boxLayout->addWidget(_iconLabel);
		_boxLayout->addSpacing(2);
		_running->hide();
	}
	_boxLayout->addWidget(_label);
#ifdef __APPLE__
	/* Hack: Fixes a bug where scrollbars would be above the lock icon */
	_boxLayout->addSpacing(16);
#endif

	Update(true);
	setLayout(_boxLayout);

	auto setRunning = [this](bool val) {
		_macro->SetPaused(!val);
	};
	connect(_running, &QAbstractButton::clicked, setRunning);
	connect(_tree->window(), SIGNAL(HighlightMacrosChanged(bool)), this,
		SLOT(EnableHighlight(bool)));
	connect(_tree->window(),
		SIGNAL(MacroRenamed(const QString &, const QString &)), this,
		SLOT(MacroRenamed(const QString &, const QString &)));
	connect(&_timer, SIGNAL(timeout()), this, SLOT(HighlightIfExecuted()));
	connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdatePaused()));
	_timer.start(1500);
}

void MacroTreeItem::EnableHighlight(bool enable)
{
	_highlight = enable;
}

void MacroTreeItem::UpdatePaused()
{
	const QSignalBlocker blocker(_running);
	_running->setChecked(!_macro->Paused());
}

void MacroTreeItem::HighlightIfExecuted()
{
	if (!_highlight || !_macro) {
		return;
	}

	if (_lastHighlightCheckTime.time_since_epoch().count() != 0 &&
	    _macro->WasExecutedSince(_lastHighlightCheckTime)) {
		HighlightWidget(this, Qt::green, QColor(0, 0, 0, 0), true);
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

void MacroTreeItem::Update(bool force)
{
	Type newType;

	if (_macro->IsGroup()) {
		newType = Type::Group;
	} else if (_macro->IsSubitem()) {
		newType = Type::SubItem;
	} else {
		newType = Type::Item;
	}

	if (!force && newType == _type) {
		return;
	}

	if (_spacer) {
		_boxLayout->removeItem(_spacer);
		delete _spacer;
		_spacer = nullptr;
	}

	if (_type == Type::Group && _expand) {
		_boxLayout->removeWidget(_expand);
		_expand->deleteLater();
		_expand = nullptr;
	}

	_type = newType;
	if (_type == Type::SubItem) {
		_spacer = new QSpacerItem(16, 1);
		_boxLayout->insertItem(0, _spacer);

	} else if (_type == Type::Group) {
		_expand = new SourceTreeSubItemCheckBox();
		_expand->setProperty("sourceTreeSubItem", true);
		_expand->setSizePolicy(QSizePolicy::Maximum,
				       QSizePolicy::Maximum);
		_expand->setMaximumSize(10, 16);
		_expand->setMinimumSize(10, 0);
#ifdef __APPLE__
		_expand->setAttribute(Qt::WA_LayoutUsesWidgetRect);
#endif
		_boxLayout->insertWidget(0, _expand);
		_expand->blockSignals(true);
		_expand->setChecked(_macro->IsCollapsed());
		_expand->blockSignals(false);
		connect(_expand, &QPushButton::toggled, this,
			&MacroTreeItem::ExpandClicked);
	} else {
		_spacer = new QSpacerItem(3, 1);
		_boxLayout->insertItem(0, _spacer);
	}
	_label->setText(QString::fromStdString(_macro->Name()));
}

void MacroTreeItem::ExpandClicked(bool checked)
{
	if (!checked) {
		_tree->GetModel()->ExpandGroup(_macro);
	} else {
		_tree->GetModel()->CollapseGroup(_macro);
	}
}

/* ========================================================================= */

void MacroTreeModel::Reset(std::deque<std::shared_ptr<Macro>> &newItems)
{
	beginResetModel();
	_macros = newItems;
	endResetModel();

	UpdateGroupState(false);
	_mt->ResetWidgets();
}

static inline int
ModelIndexToMacroIndex(int modelIdx,
		       const std::deque<std::shared_ptr<Macro>> &macros)
{
	assert((int)macros.size() >= modelIdx);
	int realIdx = 0;
	const auto &m = macros[0];
	bool inCollapsedGroup = m->IsGroup() && m->IsCollapsed();
	uint32_t groupSize = m->GroupSize();
	for (int i = 0; i < modelIdx; i++) {
		if (inCollapsedGroup) {
			realIdx += groupSize;
			groupSize = 0;
			inCollapsedGroup = false;
		}
		realIdx++;
		const auto &m = macros.at(realIdx);
		inCollapsedGroup = m->IsGroup() && m->IsCollapsed();
		groupSize = m->GroupSize();
	}
	return realIdx;
}

static inline int
MacroIndexToModelIndex(int realIdx,
		       const std::deque<std::shared_ptr<Macro>> &macros)
{
	if (realIdx == -1) {
		return -1;
	}

	int modelIdx = 0;
	bool inCollapsedGroup = false;
	uint32_t groupSize = 0;
	for (int i = 0; i < realIdx; i++) {
		if (inCollapsedGroup) {
			i += groupSize - 1;
			groupSize = 0;
			inCollapsedGroup = false;
			continue;
		}
		const auto &m = macros.at(i);
		inCollapsedGroup = m->IsGroup() && m->IsCollapsed();
		groupSize = m->GroupSize();
		modelIdx++;
	}
	return modelIdx;
}

void MacroTreeModel::MoveItemBefore(const std::shared_ptr<Macro> &item,
				    const std::shared_ptr<Macro> &before)
{
	if (!item || !before || item == before ||
	    Neighbor(item, false) == before) {
		return;
	}

	int modelFromIdx = 0, modelFromEndIdx = 0, modelTo = 0, macroFrom = 0,
	    macroTo = 0;
	try {
		modelFromEndIdx = GetItemModelIndex(item);
		modelFromIdx = modelFromEndIdx;
		modelTo = GetItemModelIndex(before);
		macroFrom = GetItemMacroIndex(item);
		macroTo = GetItemMacroIndex(before);
	} catch (const std::out_of_range &) {
		blog(LOG_WARNING,
		     "error while trying to move item \"%s\" before \"%s\"",
		     item->Name().c_str(), before->Name().c_str());
		assert(IsInValidState());
		return;
	}

	if (before->IsSubitem()) {
		modelTo -= before->IsCollapsed() ? 0 : before->GroupSize();
		macroTo -= before->GroupSize();
	}

	if (!item->IsGroup()) {
		beginMoveRows(QModelIndex(), modelFromIdx, modelFromIdx,
			      QModelIndex(), modelTo);
		auto it = std::next(_macros.begin(), macroFrom);
		std::shared_ptr<Macro> tmp = *it;
		_macros.erase(it);
		_macros.insert(std::next(_macros.begin(), macroTo), tmp);
		endMoveRows();
		assert(IsInValidState());
		return;
	}

	if (!item->IsCollapsed()) {
		modelFromEndIdx += item->GroupSize();
	}

	beginMoveRows(QModelIndex(), modelFromIdx, modelFromEndIdx,
		      QModelIndex(), modelTo);
	for (uint32_t i = 0; i <= item->GroupSize(); i++) {
		auto it = std::next(_macros.begin(), macroFrom + i);
		auto tmp = *it;
		_macros.erase(it);
		_macros.insert(std::next(_macros.begin(), macroTo + i), tmp);
	}
	endMoveRows();
	assert(IsInValidState());
}

void MacroTreeModel::MoveItemAfter(const std::shared_ptr<Macro> &item,
				   const std::shared_ptr<Macro> &after)
{
	if (!item || !after || item == after || Neighbor(item, true) == after) {
		return;
	}

	auto moveAfter = after;
	int modelFromIdx = 0, modelFromEndIdx = 0, modelTo = 0;
	try {
		modelFromIdx = GetItemModelIndex(item);
		modelFromEndIdx = modelFromIdx;
		modelTo = GetItemModelIndex(after);
	} catch (const std::out_of_range &) {
		blog(LOG_WARNING,
		     "error while trying to move item \"%s\" after \"%s\"",
		     item->Name().c_str(), after->Name().c_str());
		assert(IsInValidState());
		return;
	}

	if (after->IsGroup()) {
		modelTo += after->IsCollapsed() ? 0 : after->GroupSize();
		moveAfter = FindEndOfGroup(after, false);
	}

	if (!item->IsGroup()) {
		beginMoveRows(QModelIndex(), modelFromIdx, modelFromEndIdx,
			      QModelIndex(), modelTo + 1);
		auto it = std::find(_macros.begin(), _macros.end(), item);
		std::shared_ptr<Macro> tmp = *it;
		_macros.erase(it);
		it = std::find(_macros.begin(), _macros.end(), moveAfter);
		if (it == _macros.end()) {
			_macros.insert(it, tmp);
		} else {
			_macros.insert(std::next(it), tmp);
		}
		endMoveRows();
		assert(IsInValidState());
		return;
	}

	if (!item->IsCollapsed()) {
		modelFromEndIdx += item->GroupSize();
	}

	beginMoveRows(QModelIndex(), modelFromIdx, modelFromEndIdx,
		      QModelIndex(), modelTo + 1);
	auto groupStart = std::find(_macros.begin(), _macros.end(), item);
	auto groupEnd = std::next(groupStart, item->GroupSize() + 1);
	std::deque<std::shared_ptr<Macro>> elementsToMove(groupStart, groupEnd);
	for (const auto &element : elementsToMove) {
		auto it = std::find(_macros.begin(), _macros.end(), element);
		_macros.erase(it);
		it = std::find(_macros.begin(), _macros.end(), moveAfter);
		if (it == _macros.end()) {
			_macros.insert(it, element);
		} else {
			_macros.insert(std::next(it), element);
		}
		moveAfter = element;
	}
	endMoveRows();
	assert(IsInValidState());
}

static inline int
CountItemsVisibleInModel(const std::deque<std::shared_ptr<Macro>> &macros)
{
	int count = macros.size();
	for (const auto &m : macros) {
		if (m->IsGroup() && m->IsCollapsed()) {
			count -= m->GroupSize();
		}
	}
	return count;
}

void MacroTreeModel::Add(std::shared_ptr<Macro> item)
{
	auto lock = LockContext();
	auto idx = CountItemsVisibleInModel(_macros);
	beginInsertRows(QModelIndex(), idx, idx);
	_macros.emplace_back(item);
	endInsertRows();
	_mt->UpdateWidget(createIndex(idx, 0, nullptr), item);
	_mt->selectionModel()->clear();
	_mt->selectionModel()->select(createIndex(idx, 0, nullptr),
				      QItemSelectionModel::Select);
}

void MacroTreeModel::Remove(std::shared_ptr<Macro> item)
{
	auto lock = LockContext();
	auto uiStartIdx = GetItemModelIndex(item);
	if (uiStartIdx == -1) {
		return;
	}
	auto macroStartIdx = ModelIndexToMacroIndex(uiStartIdx, _macros);

	auto uiEndIdx = uiStartIdx;
	auto macroEndIdx = macroStartIdx;

	bool isGroup = item->IsGroup();
	if (isGroup) {
		macroEndIdx += item->GroupSize();
		if (!item->IsCollapsed()) {
			uiEndIdx += item->GroupSize();
		}
	} else if (item->IsSubitem()) {
		Macro::PrepareMoveToGroup(nullptr, item);
	}

	beginRemoveRows(QModelIndex(), uiStartIdx, uiEndIdx);
	_macros.erase(std::next(_macros.begin(), macroStartIdx),
		      std::next(_macros.begin(), macroEndIdx + 1));
	endRemoveRows();

	_mt->selectionModel()->clear();

	if (isGroup) {
		UpdateGroupState(true);
	}
	assert(IsInValidState());
}

std::shared_ptr<Macro> MacroTreeModel::Neighbor(const std::shared_ptr<Macro> &m,
						bool above) const
{
	if (!m) {
		return std::shared_ptr<Macro>();
	}

	auto it = std::find(_macros.begin(), _macros.end(), m);
	if (it == _macros.end()) {
		return std::shared_ptr<Macro>();
	}
	if (above) {
		if (it == _macros.begin()) {
			return std::shared_ptr<Macro>();
		}
		return *std::prev(it, 1);
	}
	auto result = std::next(it, 1);
	if (result == _macros.end()) {
		return std::shared_ptr<Macro>();
	}
	return *result;
}

std::shared_ptr<Macro>
MacroTreeModel::FindEndOfGroup(const std::shared_ptr<Macro> &m,
			       bool above) const
{
	auto endOfGroup = Neighbor(m, above);
	if (!endOfGroup) {
		return m;
	}

	if (above) {
		while (!endOfGroup->IsGroup()) {
			endOfGroup = Neighbor(endOfGroup, above);
		}
	} else {
		while (endOfGroup->IsSubitem() &&
		       GetItemMacroIndex(endOfGroup) + 1 !=
			       (int)_macros.size()) {
			endOfGroup = Neighbor(endOfGroup, above);
		}
		if (!endOfGroup->IsSubitem()) {
			endOfGroup = Neighbor(endOfGroup, !above);
		}
	}

	return endOfGroup;
}

std::shared_ptr<Macro> MacroTreeModel::GetCurrentMacro() const
{
	auto sel = _mt->selectionModel()->selection();
	if (sel.empty()) {
		return std::shared_ptr<Macro>();
	}
	auto idx = sel.indexes().back().row();
	if (idx >= (int)_macros.size()) {
		return std::shared_ptr<Macro>();
	}
	return _macros[ModelIndexToMacroIndex(idx, _macros)];
}

std::vector<std::shared_ptr<Macro>>
MacroTreeModel::GetCurrentMacros(const QModelIndexList &selection) const
{
	std::vector<std::shared_ptr<Macro>> result;
	result.reserve(selection.size());
	for (const auto &sel : selection) {
		try {
			result.emplace_back(_macros.at(
				ModelIndexToMacroIndex(sel.row(), _macros)));
		} catch (const std::out_of_range &) {
		}
	}
	return result;
}

MacroTreeModel::MacroTreeModel(MacroTree *st_,
			       std::deque<std::shared_ptr<Macro>> &macros)
	: QAbstractListModel(st_),
	  _mt(st_),
	  _macros(macros)
{
	UpdateGroupState(false);
}

int MacroTreeModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : CountItemsVisibleInModel(_macros);
}

QVariant MacroTreeModel::data(const QModelIndex &index, int role) const
{
	if (role == Qt::AccessibleTextRole) {
		std::shared_ptr<Macro> item =
			_macros[ModelIndexToMacroIndex(index.row(), _macros)];
		if (!item) {
			return QVariant();
		}
		return QVariant(QString::fromStdString(item->Name()));
	}

	return QVariant();
}

Qt::ItemFlags MacroTreeModel::flags(const QModelIndex &index) const
{
	if (!index.isValid()) {
		return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
	}

	std::shared_ptr<Macro> item;
	try {
		item = _macros.at(ModelIndexToMacroIndex(index.row(), _macros));
	} catch (const std::out_of_range &) {
		return QAbstractListModel::flags(index) | Qt::ItemIsDropEnabled;
	}
	bool isGroup = item->IsGroup();
	return QAbstractListModel::flags(index) | Qt::ItemIsEditable |
	       Qt::ItemIsDragEnabled |
	       (isGroup ? Qt::ItemIsDropEnabled : Qt::NoItemFlags);
}

Qt::DropActions MacroTreeModel::supportedDropActions() const
{
	return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}

QString MacroTreeModel::GetNewGroupName()
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

int MacroTreeModel::GetItemMacroIndex(const std::shared_ptr<Macro> &item) const
{
	auto it = std::find(_macros.begin(), _macros.end(), item);
	if (it == _macros.end()) {
		return -1;
	}

	return it - _macros.begin();
}

int MacroTreeModel::GetItemModelIndex(const std::shared_ptr<Macro> &item) const
{
	return MacroIndexToModelIndex(GetItemMacroIndex(item), _macros);
}

bool MacroTreeModel::IsLastItem(std::shared_ptr<Macro> item) const
{
	return GetItemModelIndex(item) + 1 == (int)_macros.size();
}

bool MacroTreeModel::IsInValidState()
{
	// Check for reordering erros
	for (size_t i = 0, j = 0; i < _macros.size(); i++) {
		const auto &m = _macros[i];
		if (QString::fromStdString(m->Name()) !=
		    data(index(j, 0), Qt::AccessibleTextRole)) {
			return false;
		}
		if (m->IsGroup() && m->IsCollapsed()) {
			i += m->GroupSize();
		}
		j++;
	}

	// Check for group errors
	for (size_t i = 0; i < _macros.size(); i++) {
		const auto &m = _macros[i];
		if (!m->IsGroup()) {
			continue;
		}
		const auto groupSize = m->GroupSize();
		assert(i + groupSize < _macros.size());
		for (uint32_t j = 1; j <= groupSize; j++) {
			assert(_macros[i + j]->IsSubitem());
		}
	}

	return true;
}

void MacroTreeModel::GroupSelectedItems(QModelIndexList &indices)
{
	if (indices.count() == 0) {
		return;
	}

	auto lock = LockContext();
	QString name = GetNewGroupName();
	std::vector<std::shared_ptr<Macro>> itemsToGroup;
	itemsToGroup.reserve(indices.size());
	int insertGroupAt = indices[0].row();
	for (int i = 0; i < indices.count(); i++) {
		int selectedIdx = indices[i].row();
		if (selectedIdx < insertGroupAt) {
			insertGroupAt = selectedIdx;
		}

		std::shared_ptr<Macro> item =
			_macros[ModelIndexToMacroIndex(selectedIdx, _macros)];
		itemsToGroup.emplace_back(item);
	}

	std::shared_ptr<Macro> group =
		Macro::CreateGroup(name.toStdString(), itemsToGroup);
	if (!group) {
		return;
	}

	// Add new list entry for group
	insertGroupAt = ModelIndexToMacroIndex(insertGroupAt, _macros);
	_macros.insert(_macros.begin() + insertGroupAt, group);

	// Move all selected items after new group entry
	int offset = 1;
	for (const auto &item : itemsToGroup) {
		auto it = _macros.begin() + GetItemMacroIndex(item);
		_macros.erase(it);
		_macros.insert(_macros.begin() + insertGroupAt + offset, item);
		offset++;
	}

	_hasGroups = true;
	_mt->selectionModel()->clear();

	Reset(_macros);
	assert(IsInValidState());
}

void MacroTreeModel::UngroupSelectedGroups(QModelIndexList &indices)
{
	if (indices.count() == 0) {
		return;
	}

	auto lock = LockContext();
	for (int i = indices.count() - 1; i >= 0; i--) {
		std::shared_ptr<Macro> item = _macros[ModelIndexToMacroIndex(
			indices[i].row(), _macros)];
		if (item->IsGroup()) {
			Macro::RemoveGroup(item);
		}
	}

	_mt->selectionModel()->clear();

	Reset(_macros);
	assert(IsInValidState());
}

void MacroTreeModel::ExpandGroup(std::shared_ptr<Macro> item)
{
	auto idx = GetItemModelIndex(item);
	if (idx == -1 || !item->IsGroup() || !item->GroupSize() ||
	    !item->IsCollapsed()) {
		return;
	}

	item->SetCollapsed(false);
	Reset(_macros);

	_mt->selectionModel()->clear();
	assert(IsInValidState());
}

void MacroTreeModel::CollapseGroup(std::shared_ptr<Macro> item)
{
	auto idx = GetItemModelIndex(item);
	if (idx == -1 || !item->IsGroup() || !item->GroupSize() ||
	    item->IsCollapsed()) {
		return;
	}

	item->SetCollapsed(true);
	Reset(_macros);

	_mt->selectionModel()->clear();
	assert(IsInValidState());
}

void MacroTreeModel::UpdateGroupState(bool update)
{
	bool nowHasGroups = false;
	for (auto &item : _macros) {
		if (item->IsGroup()) {
			nowHasGroups = true;
			break;
		}
	}

	if (nowHasGroups != _hasGroups) {
		_hasGroups = nowHasGroups;
		if (update) {
			_mt->UpdateWidgets(true);
		}
	}
}

void MacroTree::Reset(std::deque<std::shared_ptr<Macro>> &macros,
		      bool highlight)
{
	_highlight = highlight;
	MacroTreeModel *mtm = new MacroTreeModel(this, macros);
	setModel(mtm);
	GetModel()->Reset(macros);
	connect(selectionModel(),
		SIGNAL(selectionChanged(const QItemSelection &,
					const QItemSelection &)),
		this,
		SLOT(SelectionChangedHelper(const QItemSelection &,
					    const QItemSelection &)));
}

void MacroTree::Add(std::shared_ptr<Macro> item,
		    std::shared_ptr<Macro> after) const
{
	GetModel()->Add(item);
	if (after) {
		MoveItemAfter(item, after);
	}
	assert(GetModel()->IsInValidState());
}

std::shared_ptr<Macro> MacroTree::GetCurrentMacro() const
{
	return GetModel()->GetCurrentMacro();
}

std::vector<std::shared_ptr<Macro>> MacroTree::GetCurrentMacros() const
{
	QModelIndexList indices = selectedIndexes();
	return GetModel()->GetCurrentMacros(indices);
}

MacroTree::MacroTree(QWidget *parent_) : QListView(parent_)
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

	setItemDelegate(new MacroTreeDelegate(this));
}

void MacroTree::ResetWidgets()
{
	MacroTreeModel *mtm = GetModel();
	mtm->UpdateGroupState(false);
	int modelIdx = 0;
	for (int i = 0; i < (int)mtm->_macros.size(); i++) {
		QModelIndex index = mtm->createIndex(modelIdx, 0, nullptr);
		const auto &macro = mtm->_macros[i];
		setIndexWidget(index,
			       new MacroTreeItem(this, macro, _highlight));

		// Skip items of collapsed groups
		if (macro->IsGroup() && macro->IsCollapsed()) {
			i += macro->GroupSize();
		}
		modelIdx++;
	}
	assert(GetModel()->IsInValidState());
}

void MacroTree::UpdateWidget(const QModelIndex &idx,
			     std::shared_ptr<Macro> item)
{
	setIndexWidget(idx, new MacroTreeItem(this, item, _highlight));
}

void MacroTree::UpdateWidgets(bool force)
{
	MacroTreeModel *mtm = GetModel();

	for (int i = 0; i < (int)mtm->_macros.size(); i++) {
		std::shared_ptr<Macro> item = mtm->_macros[i];
		MacroTreeItem *widget = GetItemWidget(i);

		if (!widget) {
			UpdateWidget(mtm->createIndex(i, 0, nullptr), item);
		} else {
			widget->Update(force);
		}
		// Skip items of collapsed groups
		if (item->IsGroup() && item->IsCollapsed()) {
			i += item->GroupSize();
		}
	}
}

static inline void MoveItem(std::deque<std::shared_ptr<Macro>> &items,
			    std::shared_ptr<Macro> &item, int to)
{
	auto it = std::find(items.begin(), items.end(), item);
	if (it == items.end()) {
		blog(LOG_ERROR,
		     "something went wrong during drag & drop reordering");
		return;
	}

	items.erase(it);
	items.insert(std::next(items.begin(), to), item);
}

// Logic mostly based on OBS's source-tree dropEvent() with a few modifications
// regarding group handling
void MacroTree::dropEvent(QDropEvent *event)
{
	if (event->source() != this) {
		QListView::dropEvent(event);
		return;
	}

	MacroTreeModel *mtm = GetModel();
	auto &items = mtm->_macros;
	QModelIndexList indices = selectedIndexes();

	DropIndicatorPosition indicator = dropIndicatorPosition();
	int row = indexAt(
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			  event->position().toPoint()
#else
			  event->pos()
#endif
				  )
			  .row();
	bool emptyDrop = row == -1;

	if (emptyDrop) {
		if (items.empty()) {
			QListView::dropEvent(event);
			return;
		}

		row = mtm->rowCount(QModelIndex()) - 1;
		indicator = QAbstractItemView::BelowItem;
	}

	// Store destination group if moving to a group
	Macro *dropItem = items[ModelIndexToMacroIndex(row, items)]
				  .get(); // Item being dropped on
	bool itemIsGroup = dropItem->IsGroup();
	Macro *dropGroup = itemIsGroup ? dropItem : dropItem->Parent().get();

	// Not a group if moving above the group
	if (indicator == QAbstractItemView::AboveItem && itemIsGroup) {
		dropGroup = nullptr;
	}
	if (emptyDrop) {
		dropGroup = nullptr;
	}

	// Remember to remove list items if dropping on collapsed group
	bool dropOnCollapsed = false;
	if (dropGroup) {
		dropOnCollapsed = dropGroup->IsCollapsed();
	}

	if (indicator == QAbstractItemView::BelowItem ||
	    indicator == QAbstractItemView::OnItem ||
	    indicator == QAbstractItemView::OnViewport)
		row++;

	if (row < 0 || row > (int)items.size()) {
		QListView::dropEvent(event);
		return;
	}

	// Determine if any base group is selected
	bool hasGroups = false;
	for (int i = 0; i < indices.size(); i++) {
		std::shared_ptr<Macro> item =
			items[ModelIndexToMacroIndex(indices[i].row(), items)];
		if (item->IsGroup()) {
			hasGroups = true;
			break;
		}
	}

	// If dropping a group, detect if it's below another group
	std::shared_ptr<Macro> itemBelow;
	if (row == (int)items.size()) {
		itemBelow = nullptr;
	} else {
		itemBelow = items[MacroIndexToModelIndex(row, items)];
	}

	if (hasGroups) {
		if (!itemBelow || itemBelow->Parent().get() != dropGroup) {
			dropGroup = nullptr;
			dropOnCollapsed = false;
		}
	}

	// If dropping groups on other groups, disregard as invalid drag/drop
	if (dropGroup && hasGroups) {
		QListView::dropEvent(event);
		return;
	}

	// If selection includes base group items, include all group sub-items and
	// treat them all as one
	//
	// Also gather list of (sub)items to move in backend
	std::vector<std::shared_ptr<Macro>> subItemsToMove;
	QModelIndexList indicesWithoutSubitems = indices;

	if (hasGroups) {
		// Remove sub-items if selected
		for (int i = indices.size() - 1; i >= 0; i--) {
			std::shared_ptr<Macro> item =
				items[ModelIndexToMacroIndex(indices[i].row(),
							     items)];
			auto parent = item->Parent();
			if (parent != nullptr) {
				indices.removeAt(i);
			}
		}
		indicesWithoutSubitems = indices;

		// Add all sub-items of selected groups
		for (int i = indices.size() - 1; i >= 0; i--) {
			std::shared_ptr<Macro> item =
				items[ModelIndexToMacroIndex(indices[i].row(),
							     items)];
			if (!item->IsGroup()) {
				continue;
			}
			bool collapsed = item->IsCollapsed();
			for (int j = items.size() - 1; j >= 0; j--) {
				std::shared_ptr<Macro> subitem = items[j];
				auto subitemGroup = subitem->Parent().get();

				if (subitemGroup == item.get()) {
					if (!collapsed) {
						QModelIndex idx = mtm->createIndex(
							MacroIndexToModelIndex(
								j, items),
							0, nullptr);
						indices.insert(i + 1, idx);
					}
					subItemsToMove.emplace_back(subitem);
				}
			}
		}
	}

	// Build persistent indices
	QList<QPersistentModelIndex> persistentIndices;
	persistentIndices.reserve(indices.count());
	for (QModelIndex &index : indices) {
		persistentIndices.append(index);
	}
	std::sort(persistentIndices.begin(), persistentIndices.end());

	// Prepare items to move in backend
	std::sort(indicesWithoutSubitems.begin(), indicesWithoutSubitems.end());
	std::vector<std::shared_ptr<Macro>> itemsToMove;
	for (const auto &subitemsIdx : indicesWithoutSubitems) {
		auto idx = ModelIndexToMacroIndex(subitemsIdx.row(), items);
		itemsToMove.emplace_back(items[idx]);
	}

	// Move all items to destination index
	int r = row;
	for (auto &persistentIdx : persistentIndices) {
		int from = persistentIdx.row();
		int to = r;
		int itemTo = to;

		if (itemTo > from) {
			itemTo--;
		}

		if (itemTo != from) {
			mtm->beginMoveRows(QModelIndex(), from, from,
					   QModelIndex(), to);
			mtm->endMoveRows();
		}

		r = persistentIdx.row() + 1;
	}

	std::sort(persistentIndices.begin(), persistentIndices.end());
	int firstIdx = persistentIndices.front().row();
	int lastIdx = persistentIndices.back().row();

	// Remove items if dropped in to collapsed group
	if (dropOnCollapsed) {
		mtm->beginRemoveRows(QModelIndex(), firstIdx, lastIdx);
		mtm->endRemoveRows();
	}

	// Move items in backend
	auto lock = LockContext();
	int to = row;
	try {
		to = ModelIndexToMacroIndex(row, items);
	} catch (std::out_of_range const &) {
		to = items.size();
	}
	auto prev = *itemsToMove.rbegin();
	auto curIdx = mtm->GetItemMacroIndex(prev);
	int toIdx = to;
	if (toIdx > curIdx) {
		toIdx--;
	}
	Macro::PrepareMoveToGroup(dropGroup, prev);
	MoveItem(items, prev, toIdx);
	for (auto it = std::next(itemsToMove.rbegin(), 1);
	     it != itemsToMove.rend(); ++it) {
		auto &item = *it;
		auto curIdx = mtm->GetItemMacroIndex(item);
		int toIdx = mtm->GetItemMacroIndex(prev);
		if (toIdx > curIdx) {
			toIdx--;
		}
		Macro::PrepareMoveToGroup(dropGroup, item);
		MoveItem(items, item, toIdx);
		prev = item;
	}

	// Move subitems of groups
	for (const auto &i : subItemsToMove) {
		auto removeIt = std::find(items.begin(), items.end(), i);
		if (removeIt == items.end()) {
			blog(LOG_ERROR, "Cannot move subitem '%s'",
			     i->Name().c_str());
			continue;
		}
		items.erase(removeIt);
		auto targetName = i->Parent()->Name();
		auto GroupNameMatches = [targetName](std::shared_ptr<Macro> m) {
			return m->Name() == targetName;
		};
		auto it = std::find_if(items.begin(), items.end(),
				       GroupNameMatches);
		items.insert(std::next(it, 1), i);
	}

	// Update widgets and accept event
	UpdateWidgets(true);
	event->accept();
	event->setDropAction(Qt::CopyAction);

	QListView::dropEvent(event);
	assert(GetModel()->IsInValidState());
}

bool MacroTree::GroupsSelected() const
{
	if (SelectionEmpty()) {
		return false;
	}

	MacroTreeModel *mtm = GetModel();
	for (auto &idx : selectedIndexes()) {
		std::shared_ptr<Macro> item =
			mtm->_macros[ModelIndexToMacroIndex(idx.row(),
							    mtm->_macros)];
		if (item->IsGroup()) {
			return true;
		}
	}
	return false;
}

bool MacroTree::GroupedItemsSelected() const
{
	if (SelectionEmpty()) {
		return false;
	}

	auto *mtm = GetModel();
	for (auto &idx : selectedIndexes()) {
		std::shared_ptr<Macro> item =
			mtm->_macros[ModelIndexToMacroIndex(idx.row(),
							    mtm->_macros)];
		auto parent = item->Parent();
		if (parent) {
			return true;
		}
	}
	return false;
}

bool MacroTree::SingleItemSelected() const
{
	return selectedIndexes().size() == 1;
}

bool MacroTree::SelectionEmpty() const
{
	return selectedIndexes().empty();
}

void MacroTree::MoveItemBefore(const std::shared_ptr<Macro> &item,
			       const std::shared_ptr<Macro> &after) const
{
	GetModel()->MoveItemBefore(item, after);
}

void MacroTree::MoveItemAfter(const std::shared_ptr<Macro> &item,
			      const std::shared_ptr<Macro> &after) const
{
	GetModel()->MoveItemAfter(item, after);
}

MacroTreeModel *MacroTree::GetModel() const
{
	return reinterpret_cast<MacroTreeModel *>(model());
}

void MacroTree::Remove(std::shared_ptr<Macro> item) const
{
	GetModel()->Remove(item);
}

void MacroTree::Up(std::shared_ptr<Macro> item) const
{
	auto lock = LockContext();
	auto above = GetModel()->Neighbor(item, true);
	if (!above) {
		return;
	}

	if (item->IsSubitem()) {
		// Nowhere to move to, when the item above is a group or
		// regular entry
		if (!above->IsSubitem()) {
			return;
		}

		MoveItemBefore(item, above);
		return;
	}

	if (above->IsSubitem()) {
		above = GetModel()->FindEndOfGroup(above, true);
	}

	MoveItemBefore(item, above);
}

void MacroTree::Down(std::shared_ptr<Macro> item) const
{
	auto lock = LockContext();
	auto below = GetModel()->Neighbor(item, false);
	if (!below) {
		return;
	}

	if (item->IsSubitem()) {
		// Nowhere to move to, when the item below is a group or
		// regular entry
		if (!below->IsSubitem()) {
			return;
		}

		MoveItemAfter(item, below);
		return;
	}

	if (item->IsGroup()) {
		if (below->IsSubitem()) {
			below = GetModel()->FindEndOfGroup(below, false);

			// Nowhere to move group to
			if (GetModel()->IsLastItem(below)) {
				return;
			}

			below = GetModel()->Neighbor(below, false);
		}
		MoveItemAfter(item, below);
		return;
	}

	MoveItemAfter(item, below);
}

void MacroTree::GroupSelectedItems()
{
	QModelIndexList indices = selectedIndexes();
	std::sort(indices.begin(), indices.end());
	GetModel()->GroupSelectedItems(indices);
	assert(GetModel()->IsInValidState());
}

void MacroTree::UngroupSelectedGroups()
{
	QModelIndexList indices = selectedIndexes();
	GetModel()->UngroupSelectedGroups(indices);
	assert(GetModel()->IsInValidState());
}

void MacroTree::SelectionChangedHelper(const QItemSelection &,
				       const QItemSelection &)
{
	emit MacroSelectionAboutToChange();
	emit MacroSelectionChanged();
}

inline MacroTreeItem *MacroTree::GetItemWidget(int idx) const
{
	QWidget *widget = indexWidget(GetModel()->createIndex(idx, 0, nullptr));
	return reinterpret_cast<MacroTreeItem *>(widget);
}

void MacroTree::paintEvent(QPaintEvent *event)
{
	MacroTreeModel *mtm = GetModel();
	if (mtm && mtm->_macros.empty()) {
		QPainter painter(viewport());
		const QRect rectangle =
			QRect(0, 0, size().width(), size().height());
		QRect boundingRect;
		QString text(obs_module_text("AdvSceneSwitcher.macroTab.help"));
		painter.drawText(rectangle, Qt::AlignCenter | Qt::TextWordWrap,
				 text, &boundingRect);
	} else {
		QListView::paintEvent(event);
	}
}

MacroTreeDelegate::MacroTreeDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

QSize MacroTreeDelegate::sizeHint(const QStyleOptionViewItem &option,
				  const QModelIndex &index) const
{
	MacroTree *tree = qobject_cast<MacroTree *>(parent());
	QWidget *item = tree->indexWidget(index);

	if (!item) {
		return (QSize(0, 0));
	}

	return (QSize(option.widget->minimumWidth(), item->height()));
}

} // namespace advss
