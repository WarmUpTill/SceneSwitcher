#pragma once
#include "macro-condition-edit.hpp"
#include "source-selection.hpp"

#include <QWidget>
#include <QComboBox>

namespace advss {

class MacroConditionGameCapture : public MacroCondition {
public:
	MacroConditionGameCapture(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionGameCapture>(m);
	}

	SourceSelection _source;

private:
	static void HookedSignalReceived(void *data, calldata_t *);
	static void UnhookedSignalReceived(void *data, calldata_t *);

	void SetupTempVars();
	void SetupSignalHandler(obs_source_t *source);
	void SetupInitialState(obs_source_t *source);
	void GetCalldataInfo(calldata_t *cd);

	obs_source_t *_lastSource = nullptr;
	OBSSignal _hookSignal;
	OBSSignal _unhookSignal;

	bool _hooked = false;
	std::string _title = "";
	std::string _class = "";
	std::string _executable = "";
	std::mutex _mtx;

	static bool _registered;
	static const std::string id;
};

class MacroConditionGameCaptureEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionGameCaptureEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionGameCapture> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionGameCaptureEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionGameCapture>(
				cond));
	}

private slots:
	void SourceChanged(const SourceSelection &);

private:
	SourceSelectionWidget *_sources;
	std::shared_ptr<MacroConditionGameCapture> _entryData;

	bool _loading = true;
};

} // namespace advss
