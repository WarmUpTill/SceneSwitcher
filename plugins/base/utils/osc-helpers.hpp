#pragma once
#include "list-editor.hpp"
#include "variable-string.hpp"
#include "variable-number.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"

#include <variant>
#include <unordered_map>

namespace advss {

class OSCBlob {
public:
	OSCBlob() = default;
	OSCBlob(const std::string &stringRepresentation);
	void SetStringRepresentation(const StringVariable &);
	std::string GetStringRepresentation() const;
	std::optional<std::vector<char>> GetBinary() const;
	void Save(obs_data_t *obj, const char *name) const;
	void Load(obs_data_t *obj, const char *name);
	void ResolveVariables();

private:
	StringVariable _stringRep;
};

class OSCTrue {
public:
	void Save(obs_data_t *obj, const char *name) const;
	std::string GetStringRepresentation() const { return "true"; }
};

class OSCFalse {
public:
	void Save(obs_data_t *obj, const char *name) const;
	std::string GetStringRepresentation() const { return "false"; }
};

class OSCInfinity {
public:
	void Save(obs_data_t *obj, const char *name) const;
	std::string GetStringRepresentation() const { return "infinity"; }
};

class OSCNull {
public:
	void Save(obs_data_t *obj, const char *name) const;
	std::string GetStringRepresentation() const { return "null"; }
};

class OSCMessageElement {
public:
	OSCMessageElement() = default;
	OSCMessageElement(const StringVariable &v) : _value(v) {}
	OSCMessageElement(const IntVariable &v) : _value(v) {}
	OSCMessageElement(const DoubleVariable &v) : _value(v) {}
	OSCMessageElement(const OSCBlob &v) : _value(v) {}
	OSCMessageElement(const OSCTrue &v) : _value(v) {}
	OSCMessageElement(const OSCFalse &v) : _value(v) {}
	OSCMessageElement(const OSCInfinity &v) : _value(v) {}
	OSCMessageElement(const OSCNull &v) : _value(v) {}

	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	std::string ToString() const;
	const char *GetTypeName() const;
	const char *GetTypeTag() const;
	static const char *GetTypeName(const OSCMessageElement &);
	static const char *GetTypeTag(const OSCMessageElement &);

	void ResolveVariables();

private:
	struct TypeInfo {
		const char *localizedName, *tag;
	};
	static std::unordered_map<size_t, TypeInfo> _typeNames;

	std::variant<IntVariable, DoubleVariable, StringVariable, OSCBlob,
		     OSCTrue, OSCFalse, OSCInfinity, OSCNull>
		_value;

	friend class OSCMessage;
	friend class OSCMessageElementEdit;
};

class OSCMessage {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	std::string ToString() const;
	std::optional<std::vector<char>> GetBuffer() const;

	void ResolveVariables();

private:
	StringVariable _address = "/address";
	std::vector<OSCMessageElement> _elements = {
		OSCMessageElement("example"),
		OSCMessageElement(IntVariable(3))};

	friend class OSCMessageEdit;
};

class OSCMessageElementEdit : public QWidget {
	Q_OBJECT

public:
	OSCMessageElementEdit(QWidget *);
	void SetMessageElement(const OSCMessageElement &);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
	void showEvent(QShowEvent *event) override;

private slots:
	void TypeChanged(int);
	void DoubleChanged(const NumberVariable<double> &value);
	void IntChanged(const NumberVariable<int> &value);
	void TextChanged();
	void BinaryTextChanged();

signals:
	void ElementValueChanged(const OSCMessageElement &);
	void Focussed();

private:
	void SetVisibility(const OSCMessageElement &);

	QComboBox *_type;
	VariableSpinBox *_intValue;
	VariableDoubleSpinBox *_doubleValue;
	VariableLineEdit *_text;
	VariableLineEdit *_binaryText;
};

class OSCMessageEdit final : public ListEditor {
	Q_OBJECT
public:
	OSCMessageEdit(QWidget *);
	void SetMessage(const OSCMessage &);

private slots:
	void ElementValueChanged(const OSCMessageElement &);
	void ElementFocussed();
	void AddressChanged();
	void Add();
	void Remove();
	void Up();
	void Down();

signals:
	void MessageChanged(const OSCMessage &);

private:
	void InsertElement(const OSCMessageElement &);
	int GetIndexOfSignal();

	VariableLineEdit *_address;

	OSCMessage _currentSelection;
};

} // namespace advss
