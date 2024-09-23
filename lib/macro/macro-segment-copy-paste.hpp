namespace advss {

class AdvSceneSwitcher;

bool MacroSegmentIsInClipboard();
bool MacroActionIsInClipboard();
void SetCopySegmentTargetActionType(bool setToElseAction);
void SetupSegmentCopyPasteShortcutHandlers(AdvSceneSwitcher *window);

} // namespace advss
