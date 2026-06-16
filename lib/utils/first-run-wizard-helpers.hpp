#pragma once

#include "log-helper.hpp"
#include "macro.hpp"
#include "macro-action-factory.hpp"
#include "macro-condition-factory.hpp"

#include <obs-data.h>

#include <QFrame>
#include <QLabel>

#include <string>

namespace advss::wiz::detail {

static bool addCondition(advss::Macro *macro, const std::string &id,
			 obs_data_t *data)
{
	auto cond = MacroConditionFactory::Create(id, macro);
	if (!cond) {
		blog(LOG_WARNING,
		     "FirstRunWizard: condition factory returned null for '%s'",
		     id.c_str());
		return false;
	}
	if (!cond->Load(data)) {
		blog(LOG_WARNING,
		     "FirstRunWizard: condition Load() failed for '%s'",
		     id.c_str());
		return false;
	}
	macro->Conditions().emplace_back(cond);
	return true;
}

static bool addAction(advss::Macro *macro, const std::string &id,
		      obs_data_t *data)
{
	auto action = MacroActionFactory::Create(id, macro);
	if (!action) {
		blog(LOG_WARNING,
		     "FirstRunWizard: action factory returned null for '%s'",
		     id.c_str());
		return false;
	}
	if (!action->Load(data)) {
		blog(LOG_WARNING,
		     "FirstRunWizard: action Load() failed for '%s'",
		     id.c_str());
		return false;
	}
	macro->Actions().emplace_back(action);
	return true;
}

static bool addElseAction(advss::Macro *macro, const std::string &id,
			  obs_data_t *data)
{
	auto action = MacroActionFactory::Create(id, macro);
	if (!action) {
		blog(LOG_WARNING,
		     "FirstRunWizard: else-action factory returned null for '%s'",
		     id.c_str());
		return false;
	}
	if (!action->Load(data)) {
		blog(LOG_WARNING,
		     "FirstRunWizard: else-action Load() failed for '%s'",
		     id.c_str());
		return false;
	}
	macro->ElseActions().emplace_back(action);
	return true;
}

static void setupSummaryLabel(QLabel *label)
{
	label->setWordWrap(true);
	label->setTextFormat(Qt::RichText);
	label->setFrameShape(QFrame::StyledPanel);
	label->setContentsMargins(12, 12, 12, 12);
}

} // namespace advss::wiz::detail
