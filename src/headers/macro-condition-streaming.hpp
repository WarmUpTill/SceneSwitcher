#pragma once
#include "macro.hpp"
#include <QWidget>
#include <QComboBox>

enum class StreamState {
	STOP,
	START,
};

class MacroConditionStream : public MacroCondition {
public:
	bool CheckCondition();
	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	std::string GetId() { return id; };
	static std::shared_ptr<MacroCondition> Create()
	{
		return std::make_shared<MacroConditionStream>();
	}

	StreamState _streamState;

private:
	static bool _registered;
	static const std::string id;
};

class MacroConditionStreamEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionStreamEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionStream> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionStreamEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionStream>(cond));
	}

private slots:
	void StateChanged(int value);

protected:
	QComboBox *_streamState;
	std::shared_ptr<MacroConditionStream> _entryData;

private:
	bool _loading = true;
};
