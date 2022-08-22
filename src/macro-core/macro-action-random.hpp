#pragma once
#include "macro-action-edit.hpp"
#include "macro-list.hpp"

#include <QPushButton>
#include <QListWidget>
#include <unordered_map>

class MacroActionRandom : public MultiMacroRefAction {
public:
	MacroActionRandom(Macro *m) : MacroAction(m), MultiMacroRefAction(m) {}
	bool PerformAction();
	void LogAction();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionRandom>(m);
	}

	// TODO: add weights to each macro ...
	// std::unordered_map<MacroRef, int> _weights;

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

protected:
	std::shared_ptr<MacroActionRandom> _entryData;

private:
	MacroList *_list;
	bool _loading = true;
};
