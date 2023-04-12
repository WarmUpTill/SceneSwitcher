#pragma once

#include <QComboBox>
#include <QPushButton>
#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>
#include <deque>
#include <obs.hpp>
#include <websocket-helpers.hpp>

namespace advss {

class ItemSelection;
class ItemSettingsDialog;

class Item {
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
	ItemSettingsDialog(const Item &, std::deque<std::shared_ptr<Item>> &,
			   std::string_view = "AdvSceneSwitcher.item.select",
			   std::string_view = "AdvSceneSwitcher.item.select",
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
};

typedef bool (*SettingsCallback)(QWidget *, Item &);
typedef std::shared_ptr<Item> (*CreateItemFunc)();

class ItemSelection : public QWidget {
	Q_OBJECT

public:
	ItemSelection(std::deque<std::shared_ptr<Item>> &, CreateItemFunc,
		      SettingsCallback,
		      std::string_view = "AdvSceneSwitcher.item.select",
		      std::string_view = "AdvSceneSwitcher.item.select",
		      QWidget *parent = 0);
	virtual ~ItemSelection() = default;
	void SetItem(const std::string &);

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

	QComboBox *_selection;
	QPushButton *_modify;
	CreateItemFunc _create;
	SettingsCallback _askForSettings;
	std::deque<std::shared_ptr<Item>> &_items;
	std::string_view _selectStr;
	std::string_view _addStr;
};

} // namespace advss
