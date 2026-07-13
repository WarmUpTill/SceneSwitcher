#pragma once

// OBS
#include <obs-data.h>
#include <obs-frontend-api.h>
#include <obs.hpp>

// Qt
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// Standard library
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ARM64 libobs pulls in windows.h, whose macros collide with our identifiers
#ifdef _WIN32
#undef DELETE
#undef DispatchMessage
#undef SendMessage
#undef GetObject
#endif
