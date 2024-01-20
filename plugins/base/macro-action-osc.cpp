#include "macro-action-osc.hpp"

#include <QGroupBox>
#include <system_error>

namespace advss {

const std::string MacroActionOSC::id = "osc";

bool MacroActionOSC::_registered = MacroActionFactory::Register(
	MacroActionOSC::id, {MacroActionOSC::Create, MacroActionOSCEdit::Create,
			     "AdvSceneSwitcher.action.osc"});

MacroActionOSC::MacroActionOSC(Macro *m)
	: MacroAction(m),
	  _ctx(asio::io_context()),
	  _tcpSocket(asio::ip::tcp::socket(_ctx)),
	  _udpSocket(asio::ip::udp::socket(_ctx))
{
}

void MacroActionOSC::SendOSCTCPMessage(const asio::mutable_buffer &buffer)
{
	try {
		asio::write(_tcpSocket, asio::buffer(buffer));
	} catch (const std::exception &e) {
		blog(LOG_WARNING,
		     "failed to send OSC message \"%s\" via TCP %s %d: %s",
		     _message.ToString().c_str(), _ip.c_str(), _port.GetValue(),
		     e.what());
	}
}

void MacroActionOSC::SendOSCUDPMessage(const asio::mutable_buffer &buffer)
{
	try {
		_udpSocket.send_to(buffer, _updEndpoint);
	} catch (const std::exception &e) {
		blog(LOG_WARNING,
		     "failed to send OSC message \"%s\" via UDP %s %d: %s",
		     _message.ToString().c_str(), _ip.c_str(), _port.GetValue(),
		     e.what());
	}
}

void MacroActionOSC::UDPReconnect()
{
	asio::error_code ec;
	asio::ip::udp::resolver resolver(_ctx);
	auto endpoints = resolver.resolve(asio::ip::udp::v4(), _ip.c_str(),
					  std::to_string(_port.GetValue()), ec);
	if (ec) {
		endpoints = resolver.resolve(asio::ip::udp::v6(), _ip.c_str(),
					     std::to_string(_port.GetValue()),
					     ec);
	}
	if (ec) {
		blog(LOG_WARNING, "failed to get IP for \"%s\": %s",
		     _ip.c_str(), ec.message().c_str());
		return;
	}

	try {
		_updEndpoint = endpoints.begin()->endpoint();
		_udpSocket = asio::ip::udp::socket(_ctx);
		_udpSocket.open(endpoints.begin()->endpoint().protocol());
	} catch (const std::exception &e) {
		blog(LOG_WARNING, "failed to connect to UDP %s %d: %s",
		     _ip.c_str(), _port.GetValue(), e.what());
	}
}

void MacroActionOSC::TCPReconnect()
{
	asio::error_code ec;
	asio::ip::tcp::resolver resolver(_ctx);
	auto endpoints = resolver.resolve(asio::ip::tcp::v4(), _ip.c_str(),
					  std::to_string(_port.GetValue()), ec);
	if (ec) {
		endpoints = resolver.resolve(asio::ip::tcp::v6(), _ip.c_str(),
					     std::to_string(_port.GetValue()),
					     ec);
	}
	if (ec) {
		blog(LOG_WARNING, "failed to get IP for \"%s\": %s",
		     _ip.c_str(), ec.message().c_str());
		return;
	}
	try {
		_tcpSocket = asio::ip::tcp::socket(_ctx);
		_tcpSocket.connect(endpoints.begin()->endpoint());
	} catch (const std::exception &e) {
		blog(LOG_WARNING, "failed to connect to TCP %s %d: %s",
		     _ip.c_str(), _port.GetValue(), e.what());
	}
}

void MacroActionOSC::CheckReconnect()
{
	if (_protocol == Protocol::TCP &&
	    (_reconnect || !_tcpSocket.is_open())) {
		TCPReconnect();
	}

	if (_protocol == Protocol::UDP &&
	    (_reconnect || !_udpSocket.is_open())) {
		UDPReconnect();
	}
}

bool MacroActionOSC::PerformAction()
{
	auto buffer = _message.GetBuffer();
	if (!buffer.has_value()) {
		blog(LOG_WARNING, "failed to create or fill OSC buffer!");
		return true;
	}

	CheckReconnect();

	if (_protocol == Protocol::TCP &&
	    (_reconnect || !_tcpSocket.is_open())) {
		TCPReconnect();
	}

	if (_protocol == Protocol::UDP &&
	    (_reconnect || !_udpSocket.is_open())) {
		UDPReconnect();
	}

	auto rawMessage = asio::buffer(*buffer);

	switch (_protocol) {
	case MacroActionOSC::Protocol::TCP:
		SendOSCTCPMessage(rawMessage);
		break;
	case MacroActionOSC::Protocol::UDP:
		SendOSCUDPMessage(rawMessage);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionOSC::LogAction() const
{
	vblog(LOG_INFO, "sending OSC message '%s' to %s %s %d",
	      _message.ToString().c_str(),
	      _protocol == Protocol::UDP ? "UDP" : "TCP", _ip.c_str(),
	      _port.GetValue());
}

bool MacroActionOSC::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "protocol", static_cast<int>(_protocol));
	_ip.Save(obj, "ip");
	_port.Save(obj, "port");
	_message.Save(obj);
	return true;
}

bool MacroActionOSC::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_protocol = static_cast<Protocol>(obs_data_get_int(obj, "protocol"));
	_ip.Load(obj, "ip");
	_port.Load(obj, "port");
	_message.Load(obj);
	return true;
}

void MacroActionOSC::SetProtocol(Protocol p)
{
	_protocol = p;
	_reconnect = true;
}

void MacroActionOSC::SetIP(const std::string &ip)
{
	_ip = ip;
	_reconnect = true;
}

void MacroActionOSC::SetPortNr(IntVariable port)
{
	_port = port;
	_reconnect = true;
}

static void populateProtocolSelection(QComboBox *list)
{
	list->addItem("TCP");
	list->addItem("UDP");
}

MacroActionOSCEdit::MacroActionOSCEdit(
	QWidget *parent, std::shared_ptr<MacroActionOSC> entryData)
	: QWidget(parent),
	  _protocol(new QComboBox(this)),
	  _ip(new VariableLineEdit(this)),
	  _port(new VariableSpinBox(this)),
	  _message(new OSCMessageEdit(this))
{
	populateProtocolSelection(_protocol);
	_port->setMaximum(65535);

	auto networkGroup =
		new QGroupBox(obs_module_text("AdvSceneSwitcher.osc.network"));
	auto networkLayout = new QGridLayout;
	int row = 0;
	networkLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.osc.network.protocol")),
		row, 0);
	networkLayout->addWidget(_protocol, row, 1);
	++row;
	networkLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.osc.network.address")),
		row, 0);
	networkLayout->addWidget(_ip, row, 1);
	++row;
	networkLayout->addWidget(new QLabel(obs_module_text(
					 "AdvSceneSwitcher.osc.network.port")),
				 row, 0);
	networkLayout->addWidget(_port, row, 1);
	networkGroup->setLayout(networkLayout);

	auto messageGroup =
		new QGroupBox(obs_module_text("AdvSceneSwitcher.osc.message"));
	auto messageLayout = new QHBoxLayout();
	messageLayout->addWidget(_message);
	messageGroup->setLayout(messageLayout);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addWidget(networkGroup);
	mainLayout->addWidget(messageGroup);
	setLayout(mainLayout);

	QWidget::connect(_ip, SIGNAL(editingFinished()), this,
			 SLOT(IpChanged()));
	QWidget::connect(_protocol, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ProtocolChanged(int)));
	QWidget::connect(
		_port,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(PortChanged(const NumberVariable<int> &)));
	QWidget::connect(_message, SIGNAL(MessageChanged(const OSCMessage &)),
			 this, SLOT(MessageChanged(const OSCMessage &)));

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionOSCEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_protocol->setCurrentIndex(static_cast<int>(_entryData->GetProtocol()));
	_ip->setText(_entryData->GetIP());
	_port->SetValue(_entryData->GetPortNr());
	_message->SetMessage(_entryData->_message);

	adjustSize();
	updateGeometry();
}

void MacroActionOSCEdit::IpChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetIP(_ip->text().toStdString());
}

void MacroActionOSCEdit::ProtocolChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetProtocol(static_cast<MacroActionOSC::Protocol>(value));
}

void MacroActionOSCEdit::PortChanged(const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetPortNr(value);
}

void MacroActionOSCEdit::MessageChanged(const OSCMessage &m)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_message = m;

	adjustSize();
	updateGeometry();
}

} // namespace advss
