#pragma once
#include "macro-condition-edit.hpp"

#include <obs-config.h>
#include <QWidget>
#include <QComboBox>

#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)

namespace advss {

class MacroConditionScreenshot : public MacroCondition {
public:
	MacroConditionScreenshot(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionScreenshot>(m);
	}

private:
	void SetupTempVars();

	bool _screenshotTimeInitialized = false;
	std::chrono::high_resolution_clock::time_point _screenshotTime = {};
	static bool _registered;
	static const std::string id;
};

class MacroConditionScreenshotEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionScreenshotEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionScreenshot> cond = nullptr);
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionScreenshotEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionScreenshot>(
				cond));
	}

private:
	std::shared_ptr<MacroConditionScreenshot> _entryData;
};

} // namespace advss

#endif
