#pragma once
#include <obs-data.h>

namespace advss {

void AskForBackup(obs_data_t *settings);
void BackupSettingsOfCurrentVersion();

} // namespace advss
