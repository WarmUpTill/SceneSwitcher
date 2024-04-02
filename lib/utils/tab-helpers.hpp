#pragma once
#include "export-symbol-helper.hpp"

#include <obs-data.h>
#include <QTabWidget>

namespace advss {

EXPORT void AddSetupTabCallback(const char *tabName,
				std::function<QWidget *()> createWidget,
				std::function<void(QTabWidget *)> setupTab);
EXPORT void SetTabVisibleByName(QTabWidget *tabWidget, bool visible,
				const QString &name);
void SaveLastOpenedTab(QTabWidget *tabWidget);
void ResetLastOpenedTab();
void SetTabOrder(QTabWidget *tabWidget);
void SetCurrentTab(QTabWidget *tabWidget);
void SetupOtherTabs(QTabWidget *tabWidget);
void SaveTabOrder(obs_data_t *obj);
void LoadTabOrder(obs_data_t *obj);

} // namespace advss
