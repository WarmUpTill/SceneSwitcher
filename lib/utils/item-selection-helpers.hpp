#pragma once
#include "filter-combo-box.hpp"
#include "export-symbol-helper.hpp"

#include <QPushButton>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <deque>
#include <obs-data.h>

namespace advss {

class ADVSS_EXPORT ItemSelection;
class ADVSS_EXPORT ItemSettingsDialog;

class EXPORT Item {
public:
	Item(std::string name);
	Item() = default;
	virtual ~Item() = default;

	virtual void Load(obs_data_t *obj);
	virtual void Save(obs_data_t *obj) const;
	std::string Name() const { return _name; }

protected:
	std::string _name = "";

	friend ItemSelection;
	friend ItemSettingsDialog;
};

class ItemSettingsDialog : public QDialog {
	Q_OBJECT

public:
	ItemSettingsDialog(
		const Item &, std::deque<std::shared_ptr<Item>> &,
		std::string_view selectString = "AdvSceneSwitcher.item.select",
		std::string_view addString = "AdvSceneSwitcher.item.add",
		std::string_view conflictString =
			"AdvSceneSwitcher.item.nameNotAvailable",
		QWidget *parent = 0);
	virtual ~ItemSettingsDialog() = default;

private slots:
	void NameChanged(const QString &);

protected:
	void SetNameWarning(const QString);

	QLineEdit *_name;
	QLabel *_nameHint;
	QDialogButtonBox *_buttonbox;
	std::deque<std::shared_ptr<Item>> &_items;
	std::string_view _selectStr;
	std::string_view _addStr;
	std::string_view _conflictStr;
	QString _originalName;
	bool _showNameEmptyWarning = true;
};

typedef bool (*SettingsCallback)(QWidget *, Item &);
typedef std::shared_ptr<Item> (*CreateItemFunc)();

class ItemSelection : public QWidget {
	Q_OBJECT

public:
	ItemSelection(
		std::deque<std::shared_ptr<Item>> &, CreateItemFunc,
		SettingsCallback,
		std::string_view selectString = "AdvSceneSwitcher.item.select",
		std::string_view addString = "AdvSceneSwitcher.item.add",
		std::string_view conflictString =
			"AdvSceneSwitcher.item.nameNotAvailable",
		std::string_view configureTooltip = "", QWidget *parent = 0);
	virtual ~ItemSelection() = default;
	void SetItem(const std::string &);
	void ShowRenameContextMenu(bool value);

private slots:
	void ModifyButtonClicked();
	void ChangeSelection(const QString &);
	void RenameItem();
	void RenameItem(const QString &oldName, const QString &name);
	void AddItem(const QString &);
	void RemoveItem();
	void RemoveItem(const QString &);
signals:
	void SelectionChanged(const QString &);
	void ItemAdded(const QString &);
	void ItemRemoved(const QString &);
	void ItemRenamed(const QString &oldName, const QString &name);

protected:
	Item *GetCurrentItem();

	FilterComboBox *_selection;
	QPushButton *_modify;
	CreateItemFunc _create;
	SettingsCallback _askForSettings;
	std::deque<std::shared_ptr<Item>> &_items;
	std::string_view _selectStr;
	std::string_view _addStr;
	std::string_view _conflictStr;
	bool _showRenameContextMenu = true;
};

} // namespace advss
