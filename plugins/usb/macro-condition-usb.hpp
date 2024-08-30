#pragma once
#include "macro-condition-edit.hpp"
#include "usb-helpers.hpp"
#include "regex-config.hpp"

#include <QPushButton>

namespace advss {

class MacroConditionUSB : public MacroCondition {
public:
	MacroConditionUSB(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionUSB>(m);
	}

	struct DeviceMatchOption {
		std::string pattern = ".*";
		RegexConfig regex = RegexConfig(true);

		bool Matches(const std::string &value) const;
		void Save(obs_data_t *, const char *) const;
		void Load(obs_data_t *, const char *);
	};

	DeviceMatchOption _vendorID;
	DeviceMatchOption _productID;
	DeviceMatchOption _busNumber;
	DeviceMatchOption _deviceAddress;
	DeviceMatchOption _vendorName;
	DeviceMatchOption _productName;
	DeviceMatchOption _serialNumber;

private:
	void SetupTempVars();

	static bool _registered;
	static const std::string id;
};

class MacroConditionUSBEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionUSBEdit(QWidget *parent,
			      std::shared_ptr<MacroConditionUSB> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionUSBEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionUSB>(cond));
	}

private slots:
	void VendorIDChanged(const QString &text);
	void ProductIDChanged(const QString &text);
	void BusNumberChanged(const QString &text);
	void DeviceAddressChanged(const QString &text);
	void VendorNameChanged(const QString &text);
	void ProductNameChanged(const QString &text);
	void SerialNumberChanged(const QString &text);
	void VendorIDRegexChanged(const RegexConfig &regex);
	void ProductIDRegexChanged(const RegexConfig &regex);
	void BusNumberRegexChanged(const RegexConfig &regex);
	void DeviceAddressRegexChanged(const RegexConfig &regex);
	void VendorNameRegexChanged(const RegexConfig &regex);
	void ProductNameRegexChanged(const RegexConfig &regex);
	void SerialNumberRegexChanged(const RegexConfig &regex);

private:
	QComboBox *_vendorID;
	QComboBox *_productID;
	QComboBox *_busNumber;
	QComboBox *_deviceAddress;
	QComboBox *_vendorName;
	QComboBox *_productName;
	QComboBox *_serialNumber;
	RegexConfigWidget *_vendorIDRegex;
	RegexConfigWidget *_productIDRegex;
	RegexConfigWidget *_busNumberRegex;
	RegexConfigWidget *_deviceAddressRegex;
	RegexConfigWidget *_vendorNameRegex;
	RegexConfigWidget *_productNameRegex;
	RegexConfigWidget *_serialNumberRegex;

	std::shared_ptr<MacroConditionUSB> _entryData;
	bool _loading = true;
};

} // namespace advss
