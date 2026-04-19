#include "macro-export-extensions.hpp"

#include "action-queue.hpp"
#include "variable.hpp"

#include <obs.hpp>

namespace advss {

static bool setup();
static bool setupDone = setup();

static bool setup()
{
	// --- Variables ---
	AddMacroExportExtension(
		{"AdvSceneSwitcher.macroTab.export.variables", "variables",
		 [](obs_data_t *data, const QStringList &selectedIds) {
			 if (selectedIds.isEmpty()) {
				 SaveVariables(data);
				 return;
			 }
			 OBSDataArrayAutoRelease array =
				 obs_data_array_create();
			 for (const auto &v : GetVariables()) {
				 const QString name =
					 QString::fromStdString(v->Name());
				 if (!selectedIds.contains(name)) {
					 continue;
				 }
				 OBSDataAutoRelease item = obs_data_create();
				 v->Save(item);
				 obs_data_array_push_back(array, item);
			 }
			 obs_data_set_array(data, "variables", array);
		 },
		 [](obs_data_t *data, const QStringList &) {
			 ImportVariables(data);
		 },
		 []() -> QList<QPair<QString, QString>> {
			 QList<QPair<QString, QString>> items;
			 for (const auto &v : GetVariables()) {
				 const QString name =
					 QString::fromStdString(v->Name());
				 items.append({name, name});
			 }
			 return items;
		 }});

	// --- Action Queues ---
	AddMacroExportExtension(
		{"AdvSceneSwitcher.macroTab.export.actionQueues",
		 "actionQueues",
		 [](obs_data_t *data, const QStringList &selectedIds) {
			 if (selectedIds.isEmpty()) {
				 SaveActionQueues(data);
				 return;
			 }
			 OBSDataArrayAutoRelease array =
				 obs_data_array_create();
			 for (const auto &q : GetActionQueues()) {
				 const QString name =
					 QString::fromStdString(q->Name());
				 if (!selectedIds.contains(name)) {
					 continue;
				 }
				 OBSDataAutoRelease item = obs_data_create();
				 q->Save(item);
				 obs_data_array_push_back(array, item);
			 }
			 obs_data_set_array(data, "actionQueues", array);
		 },
		 [](obs_data_t *data, const QStringList &) {
			 ImportQueues(data);
		 },
		 []() -> QList<QPair<QString, QString>> {
			 QList<QPair<QString, QString>> items;
			 for (const auto &q : GetActionQueues()) {
				 const QString name =
					 QString::fromStdString(q->Name());
				 items.append({name, name});
			 }
			 return items;
		 }});

	return true;
}

} // namespace advss
