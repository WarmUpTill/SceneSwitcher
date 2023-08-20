#pragma once
#include <variable-spinbox.hpp>
#include <variable-number.hpp>
#include <variable-string.hpp>
#include <QComboBox>
#include <obs-data.h>

#include <libusb.h>

namespace advss {

QStringList GetUSBDevices();

} // namespace advss
