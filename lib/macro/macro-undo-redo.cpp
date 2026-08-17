#include "macro-undo-redo.hpp"

#include "advanced-scene-switcher.hpp"
#include "condition-logic.hpp"
#include "macro-action-factory.hpp"
#include "macro-condition-factory.hpp"
#include "macro-edit.hpp"
#include "macro-helpers.hpp"
#include "macro-segment-list.hpp"
#include "macro-settings.hpp"
#include "macro-signals.hpp"
#include "macro.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QString>

namespace advss {

using SegmentType = MacroEdit::SegmentType;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

enum class SegmentOp { ADD, REMOVE };

static const char *segmentTypeName(SegmentType type)
{
	switch (type) {
	case SegmentType::ACTION:
		return obs_module_text("AdvSceneSwitcher.undo.segment.action");
	case SegmentType::ELSE_ACTION:
		return obs_module_text(
			"AdvSceneSwitcher.undo.segment.elseAction");
	case SegmentType::CONDITION:
		return obs_module_text(
			"AdvSceneSwitcher.undo.segment.condition");
	}
	return "";
}

static std::string fmtUndoName(const char *key, const std::string &arg1)
{
	return QString(obs_module_text(key))
		.arg(QString::fromStdString(arg1))
		.toStdString();
}

static std::string fmtUndoName(const char *key, const char *arg1,
			       const std::string &arg2)
{
	return QString(obs_module_text(key))
		.arg(arg1)
		.arg(QString::fromStdString(arg2))
		.toStdString();
}

static void updateMacroEditSegment(AdvSceneSwitcher *window,
				   const std::string &macroName, SegmentOp op,
				   SegmentType type, int idx)
{
	if (!SettingsWindowIsOpened()) {
		return;
	}
	auto *const macroEdit = window->ui->macroEdit;
	const auto current = macroEdit->GetMacro();
	if (!current || current->Name() != macroName) {
		return;
	}

	// std::deque invalidates all element addresses on any insert or erase.
	// Re-point every existing widget before inserting/removing one, so none
	// hold stale pointers.
	macroEdit->SetActionData(*current);
	macroEdit->SetElseActionData(*current);
	macroEdit->SetConditionData(*current);

	if (op == SegmentOp::ADD) {
		macroEdit->InsertSegmentWidget(type, idx);
	} else {
		macroEdit->RemoveSegmentWidget(type, idx);
	}
}

// ---------------------------------------------------------------------------
// Macro-level undo/redo callbacks
// ---------------------------------------------------------------------------

// Callback data format for deletion: {"name": "MacroName"}
static void deleteMacroByName(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string name = obs_data_get_string(data, "name");
	const auto macro = GetWeakMacroByName(name.c_str()).lock();
	if (!macro) {
		return;
	}

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macroEdit->ClearSegmentWidgetCacheFor(macro.get());
		MacroSegmentList::SetCachingEnabled(false);
		window->ui->macros->Remove(macro);
		MacroSegmentList::SetCachingEnabled(true);
	} else {
		auto lock = LockContext();
		auto &macros = GetTopLevelMacros();
		const auto it = std::find(macros.begin(), macros.end(), macro);
		if (it != macros.end()) {
			macros.erase(it);
		}
	}

	MacroSignalManager::Instance()->Remove(QString::fromStdString(name));
}

// Callback data format for restoration:
// {"index": N, "parent_group": "GroupName", "macro": {...}}
static void restoreMacroFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const int index = (int)obs_data_get_int(data, "index");
	const std::string parentGroup =
		obs_data_get_string(data, "parent_group");
	OBSDataAutoRelease macroData = obs_data_get_obj(data, "macro");
	if (!macroData) {
		return;
	}

	auto macro = std::make_shared<Macro>();
	std::string macroName;
	{
		auto lock = LockContext();
		macro->Load(macroData);
		macro->PostLoad();
		RunAndClearPostLoadSteps();

		if (!parentGroup.empty()) {
			const auto parent =
				GetWeakMacroByName(parentGroup.c_str()).lock();
			if (parent) {
				Macro::PrepareMoveToGroup(parent, macro);
			}
		}

		auto &macros = GetTopLevelMacros();
		const int insertIdx = std::min(index, (int)macros.size());
		macros.insert(macros.begin() + insertIdx, macro);
		macroName = macro->Name();
	}

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macros->Reset(
			GetTopLevelMacros(),
			GetGlobalMacroSettings()._highlightExecuted);
	}

	MacroSignalManager::Instance()->Add(QString::fromStdString(macroName));
}

// ---------------------------------------------------------------------------
// Group-delete undo/redo callbacks
// ---------------------------------------------------------------------------

// Callback data format:
// {"group_name": "...", "group_index": N, "group_data": {...},
//  "children": [{"data": {...}}, ...]}
static void deleteGroupWithChildrenFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string groupName = obs_data_get_string(data, "group_name");
	const auto group = GetWeakMacroByName(groupName.c_str()).lock();
	if (!group || !group->IsGroup()) {
		return;
	}

	const auto children = GetGroupMacroEntries(group.get());

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macroEdit->ClearSegmentWidgetCacheFor(group.get());
		for (const auto &child : children) {
			window->ui->macroEdit->ClearSegmentWidgetCacheFor(
				child.get());
		}
		MacroSegmentList::SetCachingEnabled(false);
		window->ui->macros->Remove(group);
		MacroSegmentList::SetCachingEnabled(true);
	} else {
		auto lock = LockContext();
		auto &macros = GetTopLevelMacros();
		const auto it = std::find(macros.begin(), macros.end(), group);
		if (it != macros.end()) {
			macros.erase(it, std::next(it, 1 + group->GroupSize()));
		}
	}

	for (const auto &child : children) {
		MacroSignalManager::Instance()->Remove(
			QString::fromStdString(child->Name()));
	}
	MacroSignalManager::Instance()->Remove(
		QString::fromStdString(groupName));
}

static void restoreGroupWithChildrenFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const int groupIndex = (int)obs_data_get_int(data, "group_index");
	OBSDataAutoRelease groupData = obs_data_get_obj(data, "group_data");
	OBSDataArrayAutoRelease childArray =
		obs_data_get_array(data, "children");

	std::string groupName;
	bool collapsed = false;
	if (groupData) {
		groupName = obs_data_get_string(groupData, "name");
		OBSDataAutoRelease gd =
			obs_data_get_obj(groupData, "groupData");
		if (gd) {
			collapsed = obs_data_get_bool(gd, "collapsed");
		}
	}

	std::vector<std::shared_ptr<Macro>> children;
	const size_t count = obs_data_array_count(childArray);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease entry = obs_data_array_item(childArray, i);
		OBSDataAutoRelease childData = obs_data_get_obj(entry, "data");
		if (!childData) {
			continue;
		}
		auto child = std::make_shared<Macro>();
		{
			auto lock = LockContext();
			child->Load(childData);
			child->PostLoad();
			RunAndClearPostLoadSteps();
		}
		children.push_back(child);
	}

	std::string restoredGroupName;
	{
		auto lock = LockContext();
		auto group = Macro::CreateGroup(groupName, children);
		group->SetCollapsed(collapsed);
		restoredGroupName = group->Name();

		auto &macros = GetTopLevelMacros();
		const int insertIdx = std::min(groupIndex, (int)macros.size());
		macros.insert(macros.begin() + insertIdx, group);
		int offset = 1;
		for (const auto &child : children) {
			macros.insert(macros.begin() + insertIdx + offset,
				      child);
			offset++;
		}
	}

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macros->Reset(
			GetTopLevelMacros(),
			GetGlobalMacroSettings()._highlightExecuted);
	}

	MacroSignalManager::Instance()->Add(
		QString::fromStdString(restoredGroupName));
	for (const auto &child : children) {
		MacroSignalManager::Instance()->Add(
			QString::fromStdString(child->Name()));
	}
}

// ---------------------------------------------------------------------------
// Group-level undo/redo callbacks
// ---------------------------------------------------------------------------

// Callback data format for both group and ungroup:
// {"group_name": "...", "group_index": N, "children": [{"name": "..."}, ...]}
static void removeGroupFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string groupName = obs_data_get_string(data, "group_name");
	const auto group = GetWeakMacroByName(groupName.c_str()).lock();
	if (!group || !group->IsGroup()) {
		return;
	}

	{
		auto lock = LockContext();
		Macro::RemoveGroup(group);
	}

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macros->Reset(
			GetTopLevelMacros(),
			GetGlobalMacroSettings()._highlightExecuted);
	}

	MacroSignalManager::Instance()->Remove(
		QString::fromStdString(groupName));
}

static void restoreGroupFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string groupName = obs_data_get_string(data, "group_name");
	const int groupIndex = (int)obs_data_get_int(data, "group_index");
	OBSDataArrayAutoRelease childArray =
		obs_data_get_array(data, "children");

	std::vector<std::shared_ptr<Macro>> children;
	const size_t count = obs_data_array_count(childArray);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(childArray, i);
		const std::string name = obs_data_get_string(item, "name");
		const auto macro = GetWeakMacroByName(name.c_str()).lock();
		if (macro) {
			children.push_back(macro);
		}
	}

	{
		auto lock = LockContext();
		auto group = Macro::CreateGroup(groupName, children);

		auto &macros = GetTopLevelMacros();
		const int insertIdx = std::min(groupIndex, (int)macros.size());
		macros.insert(macros.begin() + insertIdx, group);

		int offset = 1;
		for (const auto &child : children) {
			const auto it =
				std::find(macros.begin(), macros.end(), child);
			if (it != macros.end()) {
				macros.erase(it);
			}
			macros.insert(macros.begin() + insertIdx + offset,
				      child);
			offset++;
		}
	}

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macros->Reset(
			GetTopLevelMacros(),
			GetGlobalMacroSettings()._highlightExecuted);
	}

	MacroSignalManager::Instance()->Add(QString::fromStdString(groupName));
}

// ---------------------------------------------------------------------------
// Segment-level undo/redo callbacks
// ---------------------------------------------------------------------------

// Callback data format for segment removal:
// {"macro": "Name", "type": N, "index": N}
static void removeSegmentFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string macroName = obs_data_get_string(data, "macro");
	const int type = (int)obs_data_get_int(data, "type");
	const int index = (int)obs_data_get_int(data, "index");

	const auto macro = GetWeakMacroByName(macroName.c_str()).lock();
	if (!macro) {
		return;
	}

	{
		auto lock = LockContext();
		switch ((SegmentType)type) {
		case SegmentType::ACTION:
			if (index >= 0 &&
			    index < (int)macro->Actions().size()) {
				macro->Actions().erase(
					macro->Actions().begin() + index);
				SetMacroAbortWait(true);
				GetMacroWaitCV().notify_all();
				macro->UpdateActionIndices();
			}
			break;
		case SegmentType::ELSE_ACTION:
			if (index >= 0 &&
			    index < (int)macro->ElseActions().size()) {
				macro->ElseActions().erase(
					macro->ElseActions().begin() + index);
				SetMacroAbortWait(true);
				GetMacroWaitCV().notify_all();
				macro->UpdateElseActionIndices();
			}
			break;
		case SegmentType::CONDITION:
			if (index >= 0 &&
			    index < (int)macro->Conditions().size()) {
				macro->Conditions().erase(
					macro->Conditions().begin() + index);
				macro->UpdateConditionIndices();
				if (index == 0 &&
				    !macro->Conditions().empty()) {
					macro->Conditions().at(0)->SetLogicType(
						Logic::Type::ROOT_NONE);
				}
			}
			break;
		}
	}

	if (auto *window = AdvSceneSwitcher::window) {
		updateMacroEditSegment(window, macroName, SegmentOp::REMOVE,
				       (SegmentType)type, index);
	}
}

// Callback data format for segment addition:
// {"macro": "Name", "type": N, "index": N, "id": "...", "logic": N,
//  "segment": {...}}
static void addSegmentFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string macroName = obs_data_get_string(data, "macro");
	const int type = (int)obs_data_get_int(data, "type");
	const int index = (int)obs_data_get_int(data, "index");
	const std::string id = obs_data_get_string(data, "id");
	const int logic = (int)obs_data_get_int(data, "logic");
	OBSDataAutoRelease segData = obs_data_get_obj(data, "segment");

	const auto macro = GetWeakMacroByName(macroName.c_str()).lock();
	if (!macro) {
		return;
	}

	{
		auto lock = LockContext();
		switch ((SegmentType)type) {
		case SegmentType::ACTION: {
			if (index < 0 || index > (int)macro->Actions().size()) {
				break;
			}
			macro->Actions().emplace(
				macro->Actions().begin() + index,
				MacroActionFactory::Create(id, macro.get()));
			if (segData) {
				macro->Actions().at(index)->Load(segData);
			}
			macro->Actions().at(index)->PostLoad();
			RunAndClearPostLoadSteps();
			macro->UpdateActionIndices();
			break;
		}
		case SegmentType::ELSE_ACTION: {
			if (index < 0 ||
			    index > (int)macro->ElseActions().size()) {
				break;
			}
			macro->ElseActions().emplace(
				macro->ElseActions().begin() + index,
				MacroActionFactory::Create(id, macro.get()));
			if (segData) {
				macro->ElseActions().at(index)->Load(segData);
			}
			macro->ElseActions().at(index)->PostLoad();
			RunAndClearPostLoadSteps();
			macro->UpdateElseActionIndices();
			break;
		}
		case SegmentType::CONDITION: {
			if (index < 0 ||
			    index > (int)macro->Conditions().size()) {
				break;
			}
			macro->Conditions().emplace(
				macro->Conditions().begin() + index,
				MacroConditionFactory::Create(id, macro.get()));
			if (segData) {
				macro->Conditions().at(index)->Load(segData);
			}
			macro->Conditions().at(index)->PostLoad();
			RunAndClearPostLoadSteps();
			macro->Conditions().at(index)->SetLogicType(
				(Logic::Type)logic);
			macro->UpdateConditionIndices();
			break;
		}
		}
	}

	if (auto *window = AdvSceneSwitcher::window) {
		updateMacroEditSegment(window, macroName, SegmentOp::ADD,
				       (SegmentType)type, index);
	}
}

// ---------------------------------------------------------------------------
// Rename undo/redo callback
// ---------------------------------------------------------------------------

// Callback data format: {"old_name": "...", "new_name": "..."}
// Used for both undo (new->old) and redo (old->new) by swapping order in
// obs_frontend_add_undo_redo_action.
static void renameMacroFromData(const char *jsonData)
{
	OBSDataAutoRelease data = obs_data_create_from_json(jsonData);
	if (!data) {
		return;
	}

	const std::string oldName = obs_data_get_string(data, "old_name");
	const std::string newName = obs_data_get_string(data, "new_name");

	const auto macro = GetWeakMacroByName(oldName.c_str()).lock();
	if (!macro) {
		return;
	}

	{
		auto lock = LockContext();
		macro->SetName(newName);
	}

	MacroSignalManager::Instance()->Rename(QString::fromStdString(oldName),
					       QString::fromStdString(newName));

	auto *const window = AdvSceneSwitcher::window;
	if (SettingsWindowIsOpened() && window) {
		window->ui->macros->Reset(
			GetTopLevelMacros(),
			GetGlobalMacroSettings()._highlightExecuted);
	}
}

// ---------------------------------------------------------------------------
// Public registration functions
// ---------------------------------------------------------------------------

void RegisterMacroAddUndoRedo(const std::string &macroName)
{
	const auto macro = GetWeakMacroByName(macroName.c_str()).lock();
	if (!macro || macro->IsGroup()) {
		return;
	}

	const auto &macros = GetTopLevelMacros();
	int index = -1;
	for (int i = 0; i < (int)macros.size(); i++) {
		if (macros[i]->Name() == macroName) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		return;
	}

	std::string parentGroup;
	if (macro->Parent()) {
		parentGroup = macro->Parent()->Name();
	}

	OBSDataAutoRelease macroData = obs_data_create();
	macro->Save(macroData);

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "name", macroName.c_str());

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_int(redoData, "index", index);
	obs_data_set_string(redoData, "parent_group", parentGroup.c_str());
	obs_data_set_obj(redoData, "macro", macroData);

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.addMacro", macroName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &deleteMacroByName,
					  &restoreMacroFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

bool RegisterMacroRemoveUndoRedo(Macro *macro)
{
	if (!macro || macro->IsGroup()) {
		return false;
	}

	const std::string macroName = macro->Name();

	const auto &macros = GetTopLevelMacros();
	int index = -1;
	for (int i = 0; i < (int)macros.size(); i++) {
		if (macros[i].get() == macro) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		return false;
	}

	std::string parentGroup;
	if (macro->Parent()) {
		parentGroup = macro->Parent()->Name();
	}

	OBSDataAutoRelease macroData = obs_data_create();
	macro->Save(macroData);

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_int(undoData, "index", index);
	obs_data_set_string(undoData, "parent_group", parentGroup.c_str());
	obs_data_set_obj(undoData, "macro", macroData);

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "name", macroName.c_str());

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.removeMacro", macroName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &restoreMacroFromData,
					  &deleteMacroByName,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
	return true;
}

void RegisterSegmentAddUndoRedo(Macro *macro, SegmentType type, int index)
{
	if (!macro) {
		return;
	}

	const std::string macroName = macro->Name();
	std::string id;
	int logic = (int)Logic::Type::ROOT_NONE;
	OBSDataAutoRelease segData = obs_data_create();

	{
		auto lock = LockContext();
		switch (type) {
		case SegmentType::ACTION:
			if (index < (int)macro->Actions().size()) {
				macro->Actions().at(index)->Save(segData);
				id = macro->Actions().at(index)->GetId();
			}
			break;
		case SegmentType::ELSE_ACTION:
			if (index < (int)macro->ElseActions().size()) {
				macro->ElseActions().at(index)->Save(segData);
				id = macro->ElseActions().at(index)->GetId();
			}
			break;
		case SegmentType::CONDITION:
			if (index < (int)macro->Conditions().size()) {
				macro->Conditions().at(index)->Save(segData);
				id = macro->Conditions().at(index)->GetId();
				logic = (int)macro->Conditions()
						.at(index)
						->GetLogicType();
			}
			break;
		}
	}

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "macro", macroName.c_str());
	obs_data_set_int(undoData, "type", (int)type);
	obs_data_set_int(undoData, "index", index);

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "macro", macroName.c_str());
	obs_data_set_int(redoData, "type", (int)type);
	obs_data_set_int(redoData, "index", index);
	obs_data_set_string(redoData, "id", id.c_str());
	obs_data_set_int(redoData, "logic", logic);
	obs_data_set_obj(redoData, "segment", segData);

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.addSegment",
			    segmentTypeName(type), macroName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &removeSegmentFromData,
					  &addSegmentFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

void RegisterSegmentRemoveUndoRedo(Macro *macro, SegmentType type, int index,
				   const std::string &segmentId,
				   obs_data_t *segmentData, int logic)
{
	if (!macro) {
		return;
	}

	const std::string macroName = macro->Name();

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "macro", macroName.c_str());
	obs_data_set_int(undoData, "type", (int)type);
	obs_data_set_int(undoData, "index", index);
	obs_data_set_string(undoData, "id", segmentId.c_str());
	obs_data_set_int(undoData, "logic", logic);
	obs_data_set_obj(undoData, "segment", segmentData);

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "macro", macroName.c_str());
	obs_data_set_int(redoData, "type", (int)type);
	obs_data_set_int(redoData, "index", index);

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.removeSegment",
			    segmentTypeName(type), macroName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &addSegmentFromData,
					  &removeSegmentFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

static OBSDataArrayAutoRelease
buildChildArray(const std::vector<std::shared_ptr<Macro>> &children)
{
	OBSDataArrayAutoRelease arr = obs_data_array_create();
	for (const auto &child : children) {
		OBSDataAutoRelease item = obs_data_create();
		obs_data_set_string(item, "name", child->Name().c_str());
		obs_data_array_push_back(arr, item);
	}
	return arr;
}

void RegisterGroupCreateUndoRedo(const std::string &groupName)
{
	const auto group = GetWeakMacroByName(groupName.c_str()).lock();
	if (!group || !group->IsGroup()) {
		return;
	}

	const auto &macros = GetTopLevelMacros();
	int groupIndex = -1;
	for (int i = 0; i < (int)macros.size(); i++) {
		if (macros[i].get() == group.get()) {
			groupIndex = i;
			break;
		}
	}
	if (groupIndex < 0) {
		return;
	}

	const auto children = GetGroupMacroEntries(group.get());
	OBSDataArrayAutoRelease childArray = buildChildArray(children);

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "group_name", groupName.c_str());
	obs_data_set_int(undoData, "group_index", groupIndex);
	obs_data_set_array(undoData, "children", childArray);

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "group_name", groupName.c_str());
	obs_data_set_int(redoData, "group_index", groupIndex);
	obs_data_set_array(redoData, "children", childArray);

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.groupMacros", groupName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &removeGroupFromData,
					  &restoreGroupFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

void RegisterGroupRemoveUndoRedo(const std::string &groupName)
{
	const auto group = GetWeakMacroByName(groupName.c_str()).lock();
	if (!group || !group->IsGroup()) {
		return;
	}

	const auto &macros = GetTopLevelMacros();
	int groupIndex = -1;
	for (int i = 0; i < (int)macros.size(); i++) {
		if (macros[i].get() == group.get()) {
			groupIndex = i;
			break;
		}
	}
	if (groupIndex < 0) {
		return;
	}

	const auto children = GetGroupMacroEntries(group.get());
	OBSDataArrayAutoRelease childArray = buildChildArray(children);

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "group_name", groupName.c_str());
	obs_data_set_int(undoData, "group_index", groupIndex);
	obs_data_set_array(undoData, "children", childArray);

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "group_name", groupName.c_str());
	obs_data_set_int(redoData, "group_index", groupIndex);
	obs_data_set_array(redoData, "children", childArray);

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.ungroupMacros", groupName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &restoreGroupFromData,
					  &removeGroupFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

void RegisterGroupDeleteUndoRedo(Macro *macro)
{
	if (!macro || !macro->IsGroup()) {
		return;
	}

	const std::string groupName = macro->Name();

	const auto &macros = GetTopLevelMacros();
	int groupIndex = -1;
	for (int i = 0; i < (int)macros.size(); i++) {
		if (macros[i].get() == macro) {
			groupIndex = i;
			break;
		}
	}
	if (groupIndex < 0) {
		return;
	}

	OBSDataAutoRelease groupData = obs_data_create();
	macro->Save(groupData);

	const auto children = GetGroupMacroEntries(macro);
	OBSDataArrayAutoRelease childArray = obs_data_array_create();
	for (const auto &child : children) {
		OBSDataAutoRelease childData = obs_data_create();
		child->Save(childData);
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_obj(entry, "data", childData);
		obs_data_array_push_back(childArray, entry);
	}

	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "group_name", groupName.c_str());
	obs_data_set_int(undoData, "group_index", groupIndex);
	obs_data_set_obj(undoData, "group_data", groupData);
	obs_data_set_array(undoData, "children", childArray);

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "group_name", groupName.c_str());
	obs_data_set_int(redoData, "group_index", groupIndex);
	obs_data_set_obj(redoData, "group_data", groupData);
	obs_data_set_array(redoData, "children", childArray);

	const std::string actionName = fmtUndoName(
		"AdvSceneSwitcher.undo.removeMacroGroup", groupName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &restoreGroupWithChildrenFromData,
					  &deleteGroupWithChildrenFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

void RegisterMacroRenameUndoRedo(const std::string &oldName,
				 const std::string &newName)
{
	OBSDataAutoRelease undoData = obs_data_create();
	obs_data_set_string(undoData, "old_name", newName.c_str());
	obs_data_set_string(undoData, "new_name", oldName.c_str());

	OBSDataAutoRelease redoData = obs_data_create();
	obs_data_set_string(redoData, "old_name", oldName.c_str());
	obs_data_set_string(redoData, "new_name", newName.c_str());

	const std::string actionName =
		fmtUndoName("AdvSceneSwitcher.undo.renameMacro", oldName);
	obs_frontend_add_undo_redo_action(actionName.c_str(),
					  &renameMacroFromData,
					  &renameMacroFromData,
					  obs_data_get_json(undoData),
					  obs_data_get_json(redoData), false);
}

} // namespace advss
