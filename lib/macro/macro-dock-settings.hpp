#pragma once
#include "obs-module-helper.hpp"
#include "variable-string.hpp"

namespace advss {

class Macro;
class MacroDock;

class MacroDockSettings {
public:
	MacroDockSettings(Macro *macro);
	~MacroDockSettings();

	void Save(obs_data_t *obj, bool saveForCopy) const;
	void Load(obs_data_t *obj);

	void EnableDock(bool);
	bool DockEnabled() const { return _registerDock; }

	bool IsStandaloneDock() const { return _standaloneDock; }
	void SetIsStandaloneDock(bool value);
	std::string DockWindowName() const { return _dockWindow; }
	void SetDockWindowName(const std::string &name);

	void SetHasRunButton(bool value);
	bool HasRunButton() const { return _hasRunButton; }
	void SetHasPauseButton(bool value);
	bool HasPauseButton() const { return _hasPauseButton; }
	void SetHasStatusLabel(bool value);
	bool HasStatusLabel() const { return _hasStatusLabel; }
	void SetHighlightEnable(bool value);
	bool HighlightEnabled() const { return _highlight; }
	StringVariable RunButtonText() const { return _runButtonText; }
	void SetRunButtonText(const std::string &text);
	StringVariable PauseButtonText() const { return _pauseButtonText; }
	void SetPauseButtonText(const std::string &text);
	StringVariable UnpauseButtonText() const { return _unpauseButtonText; }
	void SetUnpauseButtonText(const std::string &text);
	void SetConditionsTrueStatusText(const std::string &text);
	StringVariable ConditionsTrueStatusText() const;
	void SetConditionsFalseStatusText(const std::string &text);
	StringVariable ConditionsFalseStatusText() const;

	void HandleMacroNameChange();

private:
	void ResetDockIfEnabled();
	void RemoveDock();
	static std::string GenerateId();

	bool _registerDock = false;
	bool _standaloneDock = true;
	std::string _dockWindow = "Dock";
	bool _hasRunButton = true;
	bool _hasPauseButton = true;
	bool _hasStatusLabel = false;
	bool _highlight = false;
	StringVariable _runButtonText =
		obs_module_text("AdvSceneSwitcher.macroDock.run");
	StringVariable _pauseButtonText =
		obs_module_text("AdvSceneSwitcher.macroDock.pause");
	StringVariable _unpauseButtonText =
		obs_module_text("AdvSceneSwitcher.macroDock.unpause");
	StringVariable _conditionsTrueStatusText =
		obs_module_text("AdvSceneSwitcher.macroDock.statusLabel.true");
	StringVariable _conditionsFalseStatusText =
		obs_module_text("AdvSceneSwitcher.macroDock.statusLabel.false");
	std::string _id = GenerateId();
	std::string _macroName = "";

	Macro *_macro = nullptr;
	MacroDock *_dock = nullptr;
};

} // namespace advss
