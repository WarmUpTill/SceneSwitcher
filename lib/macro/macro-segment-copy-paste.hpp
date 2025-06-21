namespace advss {

class MacroEdit;

bool MacroSegmentIsInClipboard();
bool MacroActionIsInClipboard();
void SetCopySegmentTargetActionType(bool setToElseAction);
void SetupSegmentCopyPasteShortcutHandlers(MacroEdit *edit);

} // namespace advss
