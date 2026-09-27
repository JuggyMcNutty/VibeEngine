//=============================================================================
// NetMasterLink: a master server's list, asked for as DeusExGSpyLink asks
// it -- the challenge answered with Validate, then the list read to its
// end -- each server logged, the first five pinged.
//=============================================================================
class NetMasterLink extends UBrowserBufferedTcpLink;

var IpAddr MasterAddr;
var string Master;
var string GameName;
var int Found;

function Start(string Addr, int Port, string Game)
{
	ResetBuffer();
	Master = Addr;
	MasterAddr.Port = Port;
	GameName = Game;
	Resolve(Addr);
}

function Resolved(IpAddr Addr)
{
	MasterAddr.Addr = Addr.Addr;
	Log("DXCAP: master " $ Master $ " is " $ IpAddrToString(MasterAddr));
	if (BindPort() == 0)
	{
		Log("DXCAP: master: no local port");
		return;
	}
	Log("DXCAP: master: opening, link state " $ LinkState);
	Open(MasterAddr);
}

function ResolveFailed()
{
	Log("DXCAP: master: " $ Master $ " not resolved");
}

event Opened()
{
	Log("DXCAP: master: opened");
	WaitFor("\\basic\\\\secure\\", 5, 1);
}

event Closed()
{
	Log("DXCAP: master: closed after " $ Found $ " servers");
}

function Tick(float Delta)
{
	DoBufferQueueIO();
}

function GotMatch(int MatchData)
{
	local string Answer;

	switch (MatchData)
	{
	case 1:
		WaitForCount(6, 5, 2);
		break;
	case 2:
		Answer = Validate(WaitResult, GameName);
		Log("DXCAP: master: challenge " $ WaitResult $ ", answer " $ Answer);
		SendBufferedData("\\gamename\\" $ GameName $ "\\location\\0\\validate\\" $ Answer $ "\\final\\");
		SendBufferedData("\\list\\\\gamename\\" $ GameName $ "\\final\\");
		WaitFor("ip\\", 30, 3);
		break;
	case 3:
		if (WaitResult == "final\\")
			Log("DXCAP: master: list ends, " $ Found $ " servers");
		else
			WaitFor("\\", 10, 4);
		break;
	case 4:
		Found++;
		HandleServer(WaitResult);
		WaitFor("\\", 5, 3);
		break;
	}
}

function GotMatchTimeout(int MatchData)
{
	Log("DXCAP: master: timed out waiting for " $ WaitingFor $ " (step " $ MatchData $ "), after " $ Found $ " servers; input " $ Left(InputBuffer, 80));
}

function HandleServer(string Text)
{
	local string Address, Port;
	local NetPingLink Ping;

	Address = ParseDelimited(Text, ":", 1);
	Port = ParseDelimited(ParseDelimited(Text, ":", 2), "\\", 1);
	Log("DXCAP: server " $ Address $ ":" $ Port);
	if (Found <= 5)
	{
		Ping = Spawn(class'NetPingLink');
		Ping.Query(Address, int(Port));
	}
}
