#pragma once
#include "macro-edit.hpp"

#include <obs-data.h>
#include <string>

namespace advss {

class Macro;

void RegisterMacroAddUndoRedo(const std::string &macroName);
bool RegisterMacroRemoveUndoRedo(Macro *macro);
void RegisterSegmentAddUndoRedo(Macro *macro, MacroEdit::SegmentType type,
				int index);
void RegisterSegmentTypeChangeUndoRedo(Macro *macro,
				       MacroEdit::SegmentType type, int index,
				       const std::string &oldId,
				       obs_data_t *oldData, int oldLogic,
				       const std::string &newId,
				       obs_data_t *newData, int newLogic);
void RegisterSegmentRemoveUndoRedo(Macro *macro, MacroEdit::SegmentType type,
				   int index, const std::string &segmentId,
				   obs_data_t *segmentData, int logic);
void RegisterMacroRenameUndoRedo(const std::string &oldName,
				 const std::string &newName);
void RegisterGroupCreateUndoRedo(const std::string &groupName);
void RegisterGroupRemoveUndoRedo(const std::string &groupName);
void RegisterGroupDeleteUndoRedo(Macro *macro);
void RegisterMacroModifyUndoRedo(Macro *parentMacro, obs_data_t *beforeData);

} // namespace advss
