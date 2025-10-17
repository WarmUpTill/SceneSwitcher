#pragma once
#include "macro-action.hpp"
#include "macro-condition.hpp"

#include <obs.hpp>

#include <QLabel>

namespace advss {

// Retains the settings and ID of the macro segment even if they can't be
// interpreted correctly right now.
//
// As soon as a version of the plugin supporting the original ID is running it
// will take over again
template<typename T> class MacroSegmentUnknown : public T {
public:
	MacroSegmentUnknown(Macro *macro, const std::string &originalId);
	bool CheckCondition() { return false; }
	std::shared_ptr<MacroAction> Copy() const;
	bool PerformAction() { return true; };
	bool Save(obs_data_t *data) const;
	bool Load(obs_data_t *data);
	std::string GetId() const { return "unknown"; }

private:
	std::string _id;
	OBSData _settings;
};

template<typename T>
inline MacroSegmentUnknown<T>::MacroSegmentUnknown(
	Macro *macro, const std::string &originalId)
	: T(macro),
	  _id(originalId),
	  _settings(obs_data_create())
{
	obs_data_release(_settings);
}

template<typename T>
inline std::shared_ptr<MacroAction> MacroSegmentUnknown<T>::Copy() const
{
	if constexpr (std::is_same_v<T, MacroAction>) {
		return std::make_shared<MacroSegmentUnknown<T>>(
			MacroAction::GetMacro(), _id);
	}
	return {};
}

template<typename T>
inline bool MacroSegmentUnknown<T>::Save(obs_data_t *data) const
{
	T::Save(data);
	obs_data_apply(data, _settings);
	obs_data_set_string(_settings, "id", _id.c_str());
	return true;
};

template<typename T> inline bool MacroSegmentUnknown<T>::Load(obs_data_t *data)
{
	T::Load(data);
	obs_data_apply(_settings, data);
	return true;
};

inline QWidget *CreateUnknownSegmentWidget(bool isAction)
{
	return new QLabel(obs_module_text(
		isAction ? "AdvSceneSwitcher.action.unknown"
			 : "AdvSceneSwitcher.condition.unknown"));
}

} // namespace advss
