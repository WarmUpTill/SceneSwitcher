#pragma once
#include "macro-condition-edit.hpp"
#include "usb-helpers.hpp"

#include <QPushButton>
#include <QTimer>

namespace advss {

class MacroConditionUSB : public MacroCondition {
public:
	MacroConditionUSB(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionUSB>(m);
	}

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionUSBEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionUSBEdit(QWidget *parent,
			      std::shared_ptr<MacroConditionUSB> cond = nullptr);
	virtual ~MacroConditionUSBEdit();
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionUSBEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionUSB>(cond));
	}

private slots:
	void ToggleListen();

signals:
	void HeaderInfoChanged(const QString &);

private:
	std::shared_ptr<MacroConditionUSB> _entryData;
	QPushButton *_listen;
	QLabel *_test;
	bool _loading = true;
};

} // namespace advss
