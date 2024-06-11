#pragma once
#include "list-editor.hpp"
#include "macro-ref.hpp"

namespace advss {

class ADVSS_EXPORT MacroList final : public ListEditor {
	Q_OBJECT
public:
	MacroList(QWidget *parent, bool allowDuplicates, bool reorder);
	void SetContent(const std::vector<MacroRef> &);
	QAction *AddControl(QWidget *, bool addSeperator = true);
	int CurrentRow();

private slots:
	void MacroRename(const QString &oldName, const QString &newName);
	void MacroRemove(const QString &name);
	void Add();
	void Remove();
	void Up();
	void Down();
	void Clicked(QListWidgetItem *);

signals:
	void Added(const std::string &);
	void Removed(int);
	void MovedUp(int);
	void MovedDown(int);
	void Replaced(int, const std::string &);

private:
	int FindEntry(const std::string &macro);

	const bool _allowDuplicates;
};

} // namespace advss
