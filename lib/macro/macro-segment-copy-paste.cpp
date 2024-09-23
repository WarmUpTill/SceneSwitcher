#include "macro-segment-copy-paste.hpp"
#include "advanced-scene-switcher.hpp"
#include "macro.hpp"

#include <QShortcut>

namespace advss {

struct MacroSegmentCopyInfo {
	enum class Type { NONE, CONDITION, ACTION, ELSE };
	Type type = Type::NONE;
	std::shared_ptr<MacroSegment> segment;
};
static MacroSegmentCopyInfo copyInfo;

void AdvSceneSwitcher::CopyMacroSegment()
{
	copyInfo.segment.reset();
	copyInfo.type = MacroSegmentCopyInfo::Type::NONE;

	if (currentConditionIdx == -1 && currentActionIdx == -1 &&
	    currentElseActionIdx == -1) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (currentConditionIdx != -1) {
		copyInfo.type = MacroSegmentCopyInfo::Type::CONDITION;
		copyInfo.segment = macro->Conditions().at(currentConditionIdx);
	} else if (currentActionIdx != -1) {
		copyInfo.type = MacroSegmentCopyInfo::Type::ACTION;
		copyInfo.segment = macro->Actions().at(currentActionIdx);
	} else if (currentElseActionIdx != -1) {
		copyInfo.type = MacroSegmentCopyInfo::Type::ELSE;
		copyInfo.segment =
			macro->ElseActions().at(currentElseActionIdx);
	} else {
		assert(false);
	}
}

void AdvSceneSwitcher::PasteMacroSegment()
{
	if (copyInfo.type == MacroSegmentCopyInfo::Type::NONE) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro || !copyInfo.segment) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	copyInfo.segment->Save(data);

	switch (copyInfo.type) {
	case MacroSegmentCopyInfo::Type::CONDITION: {
		const auto condition = std::static_pointer_cast<MacroCondition>(
			copyInfo.segment);
		auto logic = condition->GetLogicType();
		if (logic > Logic::Type::ROOT_LAST &&
		    macro->Conditions().empty()) {
			logic = Logic::Type::ROOT_NONE;
		}
		if (logic < Logic::Type::ROOT_LAST &&
		    !macro->Conditions().empty()) {
			logic = Logic::Type::OR;
		}
		AddMacroCondition(macro.get(), macro->Conditions().size(),
				  copyInfo.segment->GetId(), data.Get(), logic);
		break;
	}
	case MacroSegmentCopyInfo::Type::ACTION:
		AddMacroAction(macro.get(), macro->Actions().size(),
			       copyInfo.segment->GetId(), data.Get());
		break;
	case MacroSegmentCopyInfo::Type::ELSE:
		AddMacroElseAction(macro.get(), macro->ElseActions().size(),
				   copyInfo.segment->GetId(), data.Get());
		break;
	default:
		break;
	}
}

bool MacroSegmentIsInClipboard()
{
	return copyInfo.type != MacroSegmentCopyInfo::Type::NONE;
}

bool MacroActionIsInClipboard()
{
	return copyInfo.type == MacroSegmentCopyInfo::Type::ACTION ||
	       copyInfo.type == MacroSegmentCopyInfo::Type::ELSE;
}

void SetCopySegmentTargetActionType(bool setToElseAction)
{
	if (copyInfo.type == MacroSegmentCopyInfo::Type::ACTION &&
	    setToElseAction) {
		copyInfo.type = MacroSegmentCopyInfo::Type::ELSE;
		return;
	}

	if (copyInfo.type == MacroSegmentCopyInfo::Type::ELSE &&
	    !setToElseAction) {
		copyInfo.type = MacroSegmentCopyInfo::Type::ACTION;
		return;
	}
}

void SetupSegmentCopyPasteShortcutHandlers(AdvSceneSwitcher *window)
{
	auto copyShortcut = new QShortcut(QKeySequence("Ctrl+C"), window);
	QWidget::connect(copyShortcut, &QShortcut::activated, window,
			 &AdvSceneSwitcher::CopyMacroSegment);
	auto pasteShortcut = new QShortcut(QKeySequence("Ctrl+V"), window);
	QWidget::connect(pasteShortcut, &QShortcut::activated, window,
			 &AdvSceneSwitcher::PasteMacroSegment);
}

} // namespace advss
