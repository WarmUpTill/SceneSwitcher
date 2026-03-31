#pragma once

namespace advss {

bool ShouldSkipPluginStartOnUncleanShutdown();

bool GetSuppressCrashDialog();
void SetSuppressCrashDialog(bool suppress);

} // namespace advss
