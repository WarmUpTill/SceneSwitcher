#include "catch.hpp"
#include "macro-test-helpers.hpp"
#include "ui-test-helpers.hpp"

#include <macro.hpp>
#include <macro-helpers.hpp>
#include <macro-search.hpp>
#include <macro-signals.hpp>
#include <macro-tree.hpp>
#include <regex-config.hpp>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace {

using namespace advss;
using namespace advss::test;

using Names = std::vector<std::string>;

const std::string newGroupName = "AdvSceneSwitcher.macroTab.defaultGroupName";

bool isHiddenByCollapsedAncestor(const Macro *macro)
{
	for (auto parent = macro->Parent(); parent; parent = parent->Parent()) {
		if (parent->IsCollapsed()) {
			return true;
		}
	}
	return false;
}

class TreeFixture {
public:
	explicit TreeFixture(const std::string &layout)
		: _window(std::make_unique<QWidget>()),
		  _tree(new MacroTree(_window.get()))
	{
		GetMacroSearchSettings() = {};
		SetupMacros(layout);

		// Same settings as in forms/advanced-scene-switcher.ui
		_tree->setDragEnabled(true);
		_tree->setDragDropMode(QAbstractItemView::InternalMove);
		_tree->setDefaultDropAction(Qt::TargetMoveAction);
		_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
		_tree->setDropIndicatorShown(true);

		auto layoutWidget = new QVBoxLayout(_window.get());
		layoutWidget->addWidget(_tree);
		_window->resize(300, 450);
		_window->show();

		Reset();
	}

	~TreeFixture()
	{
		GetMacroSearchSettings() = {};
		_window.reset();
		ProcessEvents();
		GetTopLevelMacros().clear();
	}

	MacroTree *Tree() const { return _tree; }
	QWidget *Viewport() const { return _tree->viewport(); }

	void Reset()
	{
		_tree->Reset(GetTopLevelMacros(), false);
		_checker = new ModelChecker(_tree->model(),
					    Qt::AccessibleTextRole);
		ProcessEvents();
	}

	void SetupSearch()
	{
		auto searchLayout = new QHBoxLayout();
		_searchText = new QLineEdit();
		auto clear = new QPushButton();
		auto type = new QComboBox();
		auto regex = new RegexConfigWidget();
		auto showSettings = new QPushButton();
		searchLayout->addWidget(_searchText);
		searchLayout->addWidget(clear);
		searchLayout->addWidget(type);
		searchLayout->addWidget(regex);
		searchLayout->addWidget(showSettings);
		static_cast<QVBoxLayout *>(_window->layout())
			->insertLayout(0, searchLayout);

		GetMacroSearchSettings().showAlways = true;
		auto tree = _tree;
		SetupMacroSearchWidgets(searchLayout, _searchText, clear, type,
					regex, showSettings,
					[tree]() { tree->RefreshFilter(); });
		ProcessEvents();
	}

	void Search(const QString &text)
	{
		_searchText->setText(text);
		ProcessEvents();
	}

	QModelIndex IndexOf(const std::string &name) const
	{
		return IndexOf(QString::fromStdString(name), QModelIndex());
	}

	QPoint RowCenter(const std::string &name) const
	{
		const auto index = IndexOf(name);
		REQUIRE(index.isValid());
		return _tree->visualRect(index).center();
	}

	QPoint RowTop(const std::string &name) const
	{
		const auto index = IndexOf(name);
		REQUIRE(index.isValid());
		return _tree->visualRect(index).topLeft() + QPoint(40, 1);
	}

	QPoint RowBottom(const std::string &name) const
	{
		const auto index = IndexOf(name);
		REQUIRE(index.isValid());
		return _tree->visualRect(index).bottomLeft() + QPoint(40, -1);
	}

	void ClickRow(const std::string &name,
		      Qt::KeyboardModifiers modifiers = Qt::NoModifier)
	{
		Click(Viewport(), RowCenter(name), modifiers);
	}

	void SelectRows(const Names &names)
	{
		bool first = true;
		for (const auto &name : names) {
			ClickRow(name,
				 first ? Qt::NoModifier : Qt::ControlModifier);
			first = false;
		}
	}

	void Drag(const QPoint &from, const QPoint &to)
	{
		DragAndDrop(Viewport(), from, to);
	}

	QWidget *RowWidget(const std::string &name) const
	{
		const auto index = IndexOf(name);
		REQUIRE(index.isValid());
		return _tree->indexWidget(index);
	}

	QCheckBox *ExpandCheckBox(const std::string &name) const
	{
		auto widget = RowWidget(name);
		REQUIRE(widget);
		return widget->findChild<SourceTreeSubItemCheckBox *>();
	}

	QCheckBox *RunningCheckBox(const std::string &name) const
	{
		auto widget = RowWidget(name);
		REQUIRE(widget);
		for (auto checkBox : widget->findChildren<QCheckBox *>()) {
			if (!qobject_cast<SourceTreeSubItemCheckBox *>(
				    checkBox)) {
				return checkBox;
			}
		}
		return nullptr;
	}

	void ClickCheckBox(QCheckBox *checkBox)
	{
		REQUIRE(checkBox);
		Click(checkBox, checkBox->rect().center());
	}

	Names CurrentMacros() const
	{
		Names result;
		for (const auto &macro : _tree->GetCurrentMacros()) {
			result.emplace_back(macro->Name());
		}
		std::sort(result.begin(), result.end());
		return result;
	}

	Names VisibleRows() const
	{
		std::vector<std::pair<int, std::string>> rows;
		CollectVisibleRows(QModelIndex(), rows);
		std::sort(rows.begin(), rows.end());
		Names result;
		for (const auto &[_, name] : rows) {
			result.emplace_back(name);
		}
		return result;
	}

	Names ExpectedVisibleRows() const
	{
		Names result;
		for (const auto &macro : GetTopLevelMacros()) {
			if (isHiddenByCollapsedAncestor(macro.get()) ||
			    !MacroMatchesSearchFilter(macro.get())) {
				continue;
			}
			result.emplace_back(macro->Name());
		}
		return result;
	}

	void CheckInvariants() const
	{
		ProcessEvents();
		_checker->CheckStructure();
		for (const auto &error : _checker->Errors()) {
			FAIL_CHECK(error);
		}
		CheckGroupStructure();
		REQUIRE(VisibleRows() == ExpectedVisibleRows());
		for (const auto &name : VisibleRows()) {
			CheckRowWidget(name);
		}
	}

private:
	QModelIndex IndexOf(const QString &name,
			    const QModelIndex &parent) const
	{
		auto model = _tree->model();
		for (int row = 0; row < model->rowCount(parent); row++) {
			const auto index = model->index(row, 0, parent);
			if (index.data(Qt::AccessibleTextRole).toString() ==
			    name) {
				return index;
			}
			const auto child = IndexOf(name, index);
			if (child.isValid()) {
				return child;
			}
		}
		return {};
	}

	void
	CollectVisibleRows(const QModelIndex &parent,
			   std::vector<std::pair<int, std::string>> &rows) const
	{
		auto model = _tree->model();
		for (int row = 0; row < model->rowCount(parent); row++) {
			const auto index = model->index(row, 0, parent);
			const auto rect = _tree->visualRect(index);
			if (rect.isValid() && rect.height() > 0) {
				rows.emplace_back(
					rect.top(),
					index.data(Qt::AccessibleTextRole)
						.toString()
						.toStdString());
			}
			CollectVisibleRows(index, rows);
		}
	}

	void CheckGroupStructure() const
	{
		const auto &macros = GetTopLevelMacros();
		std::vector<Macro *> openGroups;
		for (const auto &macro : macros) {
			INFO("macro \"" << macro->Name() << "\" in "
					<< DescribeMacros());
			const auto parent = macro->Parent().get();
			while (!openGroups.empty() &&
			       openGroups.back() != parent) {
				openGroups.pop_back();
			}
			if (parent) {
				REQUIRE(!openGroups.empty());
			}
			if (macro->IsGroup()) {
				openGroups.emplace_back(macro.get());
			}
		}

		for (const auto &macro : macros) {
			if (!macro->IsGroup()) {
				continue;
			}
			INFO("group \"" << macro->Name() << "\" in "
					<< DescribeMacros());
			const auto descendants = std::count_if(
				macros.begin(), macros.end(),
				[&macro](const std::shared_ptr<Macro> &m) {
					return IsMacroInGroup(m.get(),
							      macro.get());
				});
			REQUIRE(macro->GroupSize() == (uint32_t)descendants);
		}
	}

	void CheckRowWidget(const std::string &name) const
	{
		INFO("row \"" << name << "\" in " << DescribeMacros());
		auto macro = GetMacro(name);
		REQUIRE(macro);
		auto widget = RowWidget(name);
		REQUIRE(widget);

		QString label;
		for (auto child : widget->findChildren<QLabel *>()) {
			if (!child->text().isEmpty()) {
				label = child->text();
			}
		}
		REQUIRE(label.toStdString() == name);

		auto expand = ExpandCheckBox(name);
		REQUIRE(macro->IsGroup() == (expand != nullptr));
		if (expand) {
			REQUIRE(expand->isChecked() == macro->IsCollapsed());
		}

		auto running = RunningCheckBox(name);
		REQUIRE(running);
		if (!macro->IsGroup()) {
			REQUIRE(running->isChecked() == !macro->Paused());
		}
	}

	std::unique_ptr<QWidget> _window;
	MacroTree *_tree;
	ModelChecker *_checker = nullptr;
	QLineEdit *_searchText = nullptr;
};

} // namespace

TEST_CASE("Macro tree shows macros and groups", "[macro-tree]")
{
	TreeFixture f("A, G1[B, C], D, G2{E, F}, H");
	f.CheckInvariants();
	REQUIRE(f.VisibleRows() == Names{"A", "G1", "B", "C", "D", "G2", "H"});
}

TEST_CASE("Macro tree shows nested groups", "[macro-tree]")
{
	TreeFixture f("A, G1[B, G2{C, G3[D]}, E], F");
	f.CheckInvariants();
	REQUIRE(f.VisibleRows() == Names{"A", "G1", "B", "G2", "E", "F"});

	f.ClickCheckBox(f.ExpandCheckBox("G2"));
	f.CheckInvariants();
	REQUIRE(f.VisibleRows() ==
		Names{"A", "G1", "B", "G2", "C", "G3", "D", "E", "F"});

	f.Tree()->Up(GetMacro("G3"));
	f.CheckInvariants();
	REQUIRE(DescribeMacros() == "A, G1[B, G2[G3[D], C], E], F");

	f.Tree()->Remove(GetMacro("G2"));
	f.CheckInvariants();
	REQUIRE(DescribeMacros() == "A, G1[B, E], F");
}

TEST_CASE("Macro tree selection", "[macro-tree]")
{
	TreeFixture f("A, G1{B, C}, D, G2[E, F], H");

	SECTION("Single click")
	{
		f.ClickRow("E");
		REQUIRE(f.Tree()->GetCurrentMacro()->Name() == "E");
		REQUIRE(f.Tree()->SingleItemSelected());
		REQUIRE(f.Tree()->GroupedItemsSelected());
		REQUIRE_FALSE(f.Tree()->GroupsSelected());
	}

	SECTION("Rows after collapsed group")
	{
		f.ClickRow("H");
		REQUIRE(f.Tree()->GetCurrentMacro()->Name() == "H");
		f.ClickRow("D");
		REQUIRE(f.Tree()->GetCurrentMacro()->Name() == "D");
		REQUIRE_FALSE(f.Tree()->GroupedItemsSelected());
	}

	SECTION("Ctrl click")
	{
		f.ClickRow("A");
		f.ClickRow("F", Qt::ControlModifier);
		f.ClickRow("G1", Qt::ControlModifier);
		REQUIRE(f.CurrentMacros() == Names{"A", "F", "G1"});
		REQUIRE(f.Tree()->GroupsSelected());
		REQUIRE(f.Tree()->GroupedItemsSelected());
		REQUIRE_FALSE(f.Tree()->SingleItemSelected());
	}

	SECTION("Shift click")
	{
		f.ClickRow("D");
		f.ClickRow("F", Qt::ShiftModifier);
		REQUIRE(f.CurrentMacros() == Names{"D", "E", "F", "G2"});
	}

	f.CheckInvariants();
}

TEST_CASE("Macro tree expand and collapse", "[macro-tree]")
{
	TreeFixture f("A, G1[B, C], D, G2{E}");

	SECTION("Expand checkbox")
	{
		f.ClickCheckBox(f.ExpandCheckBox("G1"));
		REQUIRE(GetMacro("G1")->IsCollapsed());
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"A", "G1", "D", "G2"});

		f.ClickCheckBox(f.ExpandCheckBox("G2"));
		REQUIRE_FALSE(GetMacro("G2")->IsCollapsed());
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"A", "G1", "D", "G2", "E"});

		f.ClickCheckBox(f.ExpandCheckBox("G1"));
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() ==
			Names{"A", "G1", "B", "C", "D", "G2", "E"});
	}

	SECTION("Double click")
	{
		DoubleClick(f.Viewport(), f.RowCenter("G2"));
		REQUIRE_FALSE(GetMacro("G2")->IsCollapsed());
		f.CheckInvariants();

		DoubleClick(f.Viewport(), f.RowCenter("G1"));
		REQUIRE(GetMacro("G1")->IsCollapsed());
		f.CheckInvariants();
	}

	SECTION("Expand and collapse all")
	{
		f.Tree()->CollapseAll();
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"A", "G1", "D", "G2"});

		f.Tree()->ExpandAll();
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() ==
			Names{"A", "G1", "B", "C", "D", "G2", "E"});
	}

	SECTION("Selecting row after collapsing")
	{
		f.ClickCheckBox(f.ExpandCheckBox("G1"));
		f.ClickRow("D");
		REQUIRE(f.Tree()->GetCurrentMacro()->Name() == "D");
	}
}

TEST_CASE("Macro tree add", "[macro-tree]")
{
	TreeFixture f("A, G1[B], C, G2{D}");
	auto newMacro = std::make_shared<Macro>("X");

	SECTION("Append")
	{
		f.Tree()->Add(newMacro);
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B], C, G2{D}, X");
		REQUIRE(f.Tree()->GetCurrentMacro() == newMacro);
	}

	SECTION("After top level macro")
	{
		f.Tree()->Add(newMacro, GetMacro("A"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, X, G1[B], C, G2{D}");
	}

	SECTION("After macro following collapsed group")
	{
		f.Tree()->Add(newMacro, GetMacro("C"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B], C, X, G2{D}");
	}

	// Mirrors AdvSceneSwitcher::on_macroAdd_clicked() with a group selected
	SECTION("Into group")
	{
		auto group = GetMacro("G2");
		f.Tree()->ExpandGroup(group);
		Macro::PrepareMoveToGroup(group, newMacro);
		f.Tree()->AddToGroup(newMacro, group);
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B], C, G2[X, D]");
	}

	// Mirrors AdvSceneSwitcher::on_macroAdd_clicked() with a subitem
	// selected
	SECTION("After subitem")
	{
		Macro::PrepareMoveToGroup(GetMacro("G1"), newMacro);
		f.Tree()->Add(newMacro, GetMacro("B"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B, X], C, G2{D}");
	}
}

TEST_CASE("Macro tree remove", "[macro-tree]")
{
	TreeFixture f("A, G1[B, C], D, G2{E, F}, H");

	SECTION("Subitem")
	{
		f.Tree()->Remove(GetMacro("B"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[C], D, G2{E, F}, H");

		f.Tree()->Remove(GetMacro("C"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[], D, G2{E, F}, H");
	}

	SECTION("Expanded group")
	{
		f.Tree()->Remove(GetMacro("G1"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, D, G2{E, F}, H");
	}

	SECTION("Collapsed group")
	{
		f.Tree()->Remove(GetMacro("G2"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B, C], D, H");
	}

	SECTION("Macro after collapsed group")
	{
		f.Tree()->Remove(GetMacro("H"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B, C], D, G2{E, F}");
	}

	SECTION("Everything")
	{
		for (const auto &name : {"H", "A", "G2", "B", "G1", "D"}) {
			f.Tree()->Remove(GetMacro(name));
			f.CheckInvariants();
		}
		REQUIRE(GetTopLevelMacros().empty());
	}

	SECTION("Selected macro")
	{
		f.ClickRow("D");
		f.Tree()->Remove(f.Tree()->GetCurrentMacro());
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B, C], G2{E, F}, H");
		REQUIRE(f.Tree()->SelectionEmpty());
	}
}

TEST_CASE("Macro tree up and down", "[macro-tree]")
{
	TreeFixture f("A, B, G1[C, D], E, G2{F}, H");
	auto up = [&](const char *name) {
		f.Tree()->Up(GetMacro(name));
		f.CheckInvariants();
	};
	auto down = [&](const char *name) {
		f.Tree()->Down(GetMacro(name));
		f.CheckInvariants();
	};

	SECTION("Top level")
	{
		up("B");
		REQUIRE(DescribeMacros() == "B, A, G1[C, D], E, G2{F}, H");
		down("B");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], E, G2{F}, H");
	}

	SECTION("First and last")
	{
		up("A");
		down("H");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], E, G2{F}, H");
	}

	SECTION("Within group")
	{
		up("D");
		REQUIRE(DescribeMacros() == "A, B, G1[D, C], E, G2{F}, H");
		up("D");
		REQUIRE(DescribeMacros() == "A, B, G1[D, C], E, G2{F}, H");
		down("D");
		down("D");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], E, G2{F}, H");
	}

	SECTION("Macro past expanded group")
	{
		down("B");
		REQUIRE(DescribeMacros() == "A, G1[C, D], B, E, G2{F}, H");
		up("E");
		up("E");
		REQUIRE(DescribeMacros() == "A, E, G1[C, D], B, G2{F}, H");
	}

	SECTION("Macro past collapsed group")
	{
		down("E");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], G2{F}, E, H");
		up("E");
		up("H");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], E, H, G2{F}");
	}

	SECTION("Expanded group")
	{
		down("G1");
		REQUIRE(DescribeMacros() == "A, B, E, G1[C, D], G2{F}, H");
		down("G1");
		REQUIRE(DescribeMacros() == "A, B, E, G2{F}, G1[C, D], H");
		up("G1");
		up("G1");
		up("G1");
		REQUIRE(DescribeMacros() == "A, G1[C, D], B, E, G2{F}, H");
	}

	SECTION("Collapsed group")
	{
		up("G2");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], G2{F}, E, H");
		up("G2");
		REQUIRE(DescribeMacros() == "A, B, G2{F}, G1[C, D], E, H");
		down("G2");
		down("G2");
		down("G2");
		REQUIRE(DescribeMacros() == "A, B, G1[C, D], E, H, G2{F}");
	}
}

TEST_CASE("Macro tree group and ungroup", "[macro-tree]")
{
	SECTION("Group adjacent macros")
	{
		TreeFixture f("A, B, C, D");
		f.SelectRows({"B", "C"});
		f.Tree()->GroupSelectedItems();
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, " + newGroupName + "[B, C], D");
	}

	SECTION("Group non adjacent macros")
	{
		TreeFixture f("A, B, C, D");
		f.SelectRows({"D", "B"});
		f.Tree()->GroupSelectedItems();
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, " + newGroupName + "[B, D], C");
	}

	SECTION("Group after collapsed group")
	{
		TreeFixture f("G1{X}, A, B, C");
		f.SelectRows({"B", "C"});
		f.Tree()->GroupSelectedItems();
		f.CheckInvariants();
		REQUIRE(DescribeMacros() ==
			"G1{X}, A, " + newGroupName + "[B, C]");
	}

	SECTION("Ungroup expanded group")
	{
		TreeFixture f("A, G1[B, C], D");
		f.SelectRows({"G1"});
		f.Tree()->UngroupSelectedGroups();
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, B, C, D");
	}

	SECTION("Ungroup collapsed group")
	{
		TreeFixture f("A, G1{B, C}, D");
		f.SelectRows({"G1"});
		f.Tree()->UngroupSelectedGroups();
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, B, C, D");
		REQUIRE(f.VisibleRows() == Names{"A", "B", "C", "D"});
	}

	SECTION("Ungroup multiple groups")
	{
		TreeFixture f("G1{A}, B, G2[C, D]");
		f.SelectRows({"G1", "G2"});
		f.Tree()->UngroupSelectedGroups();
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, B, C, D");
	}
}

TEST_CASE("Macro tree drag and drop", "[macro-tree]")
{
	if (!DragAndDropSupported()) {
		WARN("Skipped: drag and drop is not supported by the Qt platform");
		return;
	}

	SECTION("Macro below macro")
	{
		TreeFixture f("A, B, C, D");
		f.Drag(f.RowCenter("A"), f.RowBottom("C"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "B, C, A, D");
	}

	SECTION("Macro above macro")
	{
		TreeFixture f("A, B, C, D");
		f.Drag(f.RowCenter("D"), f.RowTop("A"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "D, A, B, C");
	}

	SECTION("Macro below last row")
	{
		TreeFixture f("A, B, C");
		f.Drag(f.RowCenter("A"),
		       QPoint(40, f.Viewport()->height() - 5));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "B, C, A");
	}

	SECTION("Macro onto expanded group")
	{
		TreeFixture f("A, G1[B, C], D");
		f.Drag(f.RowCenter("A"), f.RowCenter("G1"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "G1[A, B, C], D");
	}

	SECTION("Macro between subitems")
	{
		TreeFixture f("A, G1[B, C], D");
		f.Drag(f.RowCenter("D"), f.RowTop("C"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[B, D, C]");
	}

	SECTION("Subitem out of group")
	{
		TreeFixture f("A, G1[B, C], D");
		f.Drag(f.RowCenter("B"), f.RowBottom("D"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, G1[C], D, B");
	}

	SECTION("Subitem to top")
	{
		TreeFixture f("A, G1[B, C], D");
		f.Drag(f.RowCenter("C"), f.RowTop("A"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "C, A, G1[B], D");
	}

	SECTION("Macro past collapsed group")
	{
		TreeFixture f("A, G1{B, C}, D");
		f.Drag(f.RowCenter("D"), f.RowTop("A"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "D, A, G1{B, C}");
	}

	SECTION("Expanded group")
	{
		TreeFixture f("A, G1[B, C], D, E");
		f.Drag(f.RowCenter("G1"), f.RowBottom("D"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, D, G1[B, C], E");
	}

	SECTION("Collapsed group")
	{
		TreeFixture f("A, G1{B, C}, D, E");
		f.Drag(f.RowCenter("G1"), f.RowBottom("D"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "A, D, G1{B, C}, E");
	}

	SECTION("Group onto group is rejected")
	{
		TreeFixture f("G1[A], G2[B]");
		f.Drag(f.RowCenter("G2"), f.RowCenter("G1"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "G1[A], G2[B]");
	}

	SECTION("Multiple macros")
	{
		TreeFixture f("A, B, C, D");
		f.SelectRows({"A", "C"});
		f.Drag(f.RowCenter("C"), f.RowBottom("D"));
		f.CheckInvariants();
		REQUIRE(DescribeMacros() == "B, D, A, C");
	}
}

TEST_CASE("Macro tree drop onto collapsed group", "[macro-tree]")
{
	if (!DragAndDropSupported()) {
		WARN("Skipped: drag and drop is not supported by the Qt platform");
		return;
	}

	TreeFixture f("A, G1{B, C}, D");
	f.Drag(f.RowCenter("D"), f.RowCenter("G1"));
	f.CheckInvariants();
	REQUIRE(DescribeMacros() == "A, G1{D, B, C}");
}

TEST_CASE("Macro tree search", "[macro-tree]")
{
	SECTION("Filter rows")
	{
		TreeFixture f("A1, G1[B1, C2], D2, G2{E1}, G3[F1]");
		f.SetupSearch();
		f.Search("2");
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"G1", "C2", "D2"});

		f.Search("");
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() ==
			Names{"A1", "G1", "B1", "C2", "D2", "G2", "G3", "F1"});
	}

	SECTION("Filter rows after collapsed group")
	{
		TreeFixture f("G1{X1}, A1, B2, C2");
		f.SetupSearch();
		f.Search("2");
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"B2", "C2"});
	}

	SECTION("Filter survives modifications")
	{
		TreeFixture f("A1, G1[B1, C2], D2, E2");
		f.SetupSearch();
		f.Search("2");
		f.Tree()->Remove(GetMacro("D2"));
		f.CheckInvariants();
		f.Tree()->Up(GetMacro("E2"));
		f.CheckInvariants();
		f.ClickCheckBox(f.ExpandCheckBox("G1"));
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"E2", "G1"});
		f.Tree()->Add(std::make_shared<Macro>("X1"));
		f.CheckInvariants();
		f.Tree()->Add(std::make_shared<Macro>("Y2"));
		f.CheckInvariants();
		REQUIRE(f.VisibleRows() == Names{"E2", "G1", "Y2"});
	}
}

TEST_CASE("Macro tree running state", "[macro-tree]")
{
	SECTION("Group checkbox pauses all subitems")
	{
		TreeFixture f("A, G1[B, C]");
		auto groupCheckBox = f.RunningCheckBox("G1");
		REQUIRE(groupCheckBox->checkState() == Qt::Checked);

		f.ClickCheckBox(groupCheckBox);
		REQUIRE(GetMacro("B")->Paused());
		REQUIRE(GetMacro("C")->Paused());
		REQUIRE_FALSE(GetMacro("A")->Paused());
		REQUIRE(groupCheckBox->checkState() == Qt::Unchecked);
		f.CheckInvariants();
	}

	SECTION("Subitem checkbox updates group")
	{
		TreeFixture f("A, G1[B, C]");
		f.ClickCheckBox(f.RunningCheckBox("B"));
		REQUIRE(GetMacro("B")->Paused());
		REQUIRE(f.RunningCheckBox("G1")->checkState() ==
			Qt::PartiallyChecked);
		f.CheckInvariants();
	}

	SECTION("Update after backend change")
	{
		TreeFixture f("G0{X}, A, B");
		GetMacro("B")->SetPaused(true);
		f.Tree()->UpdateRunningStates();
		f.CheckInvariants();
	}
}

TEST_CASE("Macro tree running state after collapsed group", "[macro-tree]")
{
	TreeFixture f("G0{X}, A, G1[B, C]");
	f.ClickCheckBox(f.RunningCheckBox("B"));
	REQUIRE(GetMacro("B")->Paused());
	REQUIRE(f.RunningCheckBox("G1")->checkState() == Qt::PartiallyChecked);
}

TEST_CASE("Macro tree rename", "[macro-tree]")
{
	TreeFixture f("A, G1{B}, C");
	GetMacro("C")->SetName("Z");
	emit MacroSignalManager::Instance()->Rename("C", "Z");
	f.CheckInvariants();
	REQUIRE(f.VisibleRows() == Names{"A", "G1", "Z"});
}
