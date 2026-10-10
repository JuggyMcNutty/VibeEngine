#pragma once

#include <functional>
#include <string>

// The line to the launcher that started the engine, when one did: the
// recreated DeusEx, which stays for the game's whole run as the original's
// process does (deusex-launcher main's README, "The game and the launcher").
// Its descriptor is DXL_LAUNCHER_FD in the environment; one text line a
// message. The engine says "hello" as it starts and "ready" once its first
// map is in -- the launcher's splash closes then -- and takes "TakeFocus"
// and "Open <url>", what a second launch forwarded. "relaunch <line>" asks
// the launcher to start again with that command line once the game has
// exited, as the original's RELAUNCH starts DeusEx.exe. Without the variable
// every call does nothing.
class LauncherLine
{
public:
	static LauncherLine& Get();

	void Hello();
	void Ready();
	void Relaunch(const std::string& commandLine);

	// Whether a launcher started the engine and is still on the line.
	bool Present() { return Open(); }

	// Each whole line that has come in, to onLine; never waits.
	void Poll(const std::function<void(const std::string&)>& onLine);

private:
	bool Open();
	void Send(const std::string& line);

	int fd = -1;
	bool tried = false;
	std::string pending;
};
