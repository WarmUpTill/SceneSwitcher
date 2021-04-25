#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

class MacroConditionRegion : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	int GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionRegion>();
	}

	int _minX = 0, _minY = 0, _maxX = 0, _maxY = 0;

private:
	static bool _registered;
	static const int id;
};

class MacroConditionRegionEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionRegionEdit(
		std::shared_ptr<MacroConditionRegion> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionRegionEdit(
			std::dynamic_pointer_cast<MacroConditionRegion>(cond));
	}

private slots:
	void MinXChanged(int pos);
	void MinYChanged(int pos);
	void MaxXChanged(int pos);
	void MaxYChanged(int pos);

protected:
	QSpinBox *_minX;
	QSpinBox *_minY;
	QSpinBox *_maxX;
	QSpinBox *_maxY;
	std::shared_ptr<MacroConditionRegion> _entryData;

private:
	bool _loading = true;
};
