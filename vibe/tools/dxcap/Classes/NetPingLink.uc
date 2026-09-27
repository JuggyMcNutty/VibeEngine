//=============================================================================
// NetPingLink: a server asked for its status over UDP, as DeusExServerPing
// asks, its answer logged.
//=============================================================================
class NetPingLink extends UdpLink;

var IpAddr Server;
var string Host;

// A few of InternetLink's conversions and GameSpy answers, logged.
function SelfCheck()
{
	local IpAddr A;

	Log("DXCAP: validate abcdef deusex = " $ Validate("abcdef", "deusex"));
	Log("DXCAP: validate abcdef ut = " $ Validate("abcdef", "ut"));
	Log("DXCAP: validate Q9w3Xk unreal = " $ Validate("Q9w3Xk", "unreal"));
	Log("DXCAP: validate zzzzzz oldver = " $ Validate("zzzzzz", "oldver"));
	if (StringToIpAddr("10.1.2.3", A))
		Log("DXCAP: 10.1.2.3 is " $ A.Addr $ " port " $ A.Port $ ", back " $ IpAddrToString(A));
	Log("DXCAP: 10.1.2.3:7790 converts " $ StringToIpAddr("10.1.2.3:7790", A));
	A.Addr = -1062731775;
	A.Port = 7791;
	Log("DXCAP: -1062731775:7791 prints " $ IpAddrToString(A));
	GetLocalIP(A);
	Log("DXCAP: local address " $ IpAddrToString(A));
	Log("DXCAP: link mode " $ LinkMode $ ", receive mode " $ ReceiveMode);
}

function Query(string Address, int QueryPort)
{
	Host = Address $ ":" $ QueryPort;
	if (!StringToIpAddr(Address, Server))
	{
		Log("DXCAP: ping: no address in " $ Address);
		return;
	}
	Server.Port = QueryPort;
	if (BindPort(2000, true) == 0)
	{
		Log("DXCAP: ping: no local port");
		return;
	}
	Log("DXCAP: ping " $ Host $ " sent " $ SendText(Server, "\\status\\"));
}

event ReceivedText(IpAddr Addr, string Text)
{
	Log("DXCAP: reply from " $ IpAddrToString(Addr) $ ", " $ Len(Text) $ " characters: " $ Left(Text, 160));
}
