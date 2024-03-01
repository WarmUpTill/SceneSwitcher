#pragma once
#include "macro-action-edit.hpp"
#include "macro-list.hpp"

#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>

namespace advss {

class MacroActionRandom : public MultiMacroRefAction {
public:
	MacroActionRandom(Macro *m) : MacroAction(m), MultiMacroRefAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;

	bool _allowRepeat = false;

private:
	MacroRef lastRandomMacro;
	static bool _registered;
	static const std::string id;
};

class MacroActionRandomEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionRandomEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionRandom> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionRandomEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionRandom>(action));
	}

private slots:
	void MacroRemove(const QString &name);
	void Add(const std::string &);
	void Remove(int);
	void Replace(int, const std::string &);
	void AllowRepeatChanged(int);

protected:
	std::shared_ptr<MacroActionRandom> _entryData;

private:
	bool ShouldShowAllowRepeat();

	MacroList *_list;
	QCheckBox *_allowRepeat;
	bool _loading = true;
};

} // namespace advss
