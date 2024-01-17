#pragma once
#include "macro-ref.hpp"

#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>

namespace advss {

class ADVSS_EXPORT MacroList : public QWidget {
	Q_OBJECT
public:
	MacroList(QWidget *parent, bool allowDuplicates, bool reorder);
	void SetContent(const std::vector<MacroRef> &);
	void AddControl(QWidget *);
	int CurrentRow();

private slots:
	void MacroRename(const QString &oldName, const QString &newName);
	void MacroRemove(const QString &name);
	void Add();
	void Remove();
	void Up();
	void Down();
	void MacroItemClicked(QListWidgetItem *);

signals:
	void Added(const std::string &);
	void Removed(int);
	void MovedUp(int);
	void MovedDown(int);
	void Replaced(int, const std::string &);

private:
	int FindEntry(const std::string &macro);
	void SetMacroListSize();

	QListWidget *_list;
	QPushButton *_add;
	QPushButton *_remove;
	QPushButton *_up;
	QPushButton *_down;
	QHBoxLayout *_controlsLayout;
	const bool _allowDuplicates;
	const bool _reorder;
};

} // namespace advss
