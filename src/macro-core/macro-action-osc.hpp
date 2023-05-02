#pragma once
#include "macro-action-edit.hpp"
#include "osc-helpers.hpp"

#include <memory>
#include <asio.hpp>

namespace advss {

class MacroActionOSC : public MacroAction {
public:
	MacroActionOSC(Macro *m);
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionOSC>(m);
	}

	enum class Protocol {
		TCP,
		UDP,
	};

	void SetProtocol(Protocol);
	Protocol GetProtocol() const { return _protocol; }
	void SetIP(const std::string &);
	StringVariable GetIP() const { return _ip; }
	void SetPortNr(IntVariable);
	IntVariable GetPortNr() { return _port; }

	OSCMessage _message;

private:
	void SendOSCTCPMessage(const asio::mutable_buffer &);
	void SendOSCUDPMessage(const asio::mutable_buffer &);

	void CheckReconnect();
	void TCPReconnect();
	void UDPReconnect();

	Protocol _protocol = Protocol::UDP;
	StringVariable _ip = "localhost";
	IntVariable _port = 12345;
	bool _reconnect = true;

	asio::io_context _ctx;
	asio::ip::tcp::socket _tcpSocket;
	asio::ip::udp::socket _udpSocket;
	asio::ip::udp::endpoint _updEndpoint;

	static bool _registered;
	static const std::string id;
};

class MacroActionOSCEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionOSCEdit(QWidget *parent,
			   std::shared_ptr<MacroActionOSC> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionOSCEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionOSC>(action));
	}

private slots:
	void IpChanged();
	void MessageChanged(const OSCMessage &);
	void ProtocolChanged(int);
	void PortChanged(const NumberVariable<int> &value);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionOSC> _entryData;

private:
	QComboBox *_protocol;
	VariableLineEdit *_ip;
	VariableSpinBox *_port;
	OSCMessageEdit *_message;
	bool _loading = true;
};

} // namespace advss
