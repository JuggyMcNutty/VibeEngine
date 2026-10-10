#pragma once

#include <string>
#include <vector>

// A run driven from outside the game's script, for the harness's live mode
// (vibe/tools/dxcap.sh live, --timeline=<file>): a server's game drops a
// player whose console is not the stock one, so a live run keeps it, and
// this presses keys and runs console commands at set times instead. Each
// line of the file is "<clock> <seconds> <action>": the clock "start" counts
// from the engine's start, "game" the seconds of a net game this client is
// in; the action "press <key>" or "release <key>" as the key in the window
// (a mouse button as pressed at the pointer, a press of MouseWheelUp or
// MouseWheelDown a notch of the wheel), "pointer <x> <y>" the root
// window's pointer moved there, else a console command (shot, exit, ...). Each second of the game where
// the player stands is logged, and every other pawn's place and animation,
// as JoinConsole logs them; dropped back to the menu, the run exits.
class Timeline
{
public:
	bool Load(const std::string& path);
	void Tick(float realElapsed);

private:
	struct Action
	{
		bool InGame = false;
		float Time = 0.0f;
		std::string Verb;
		std::string Arg;
		bool Done = false;
	};

	void Run(Action& action);
	void LogPlaces();

	std::vector<Action> Actions;
	float StartTime = 0.0f;
	float GameTime = 0.0f;
	float LogTime = 0.0f;
	bool WasInGame = false;
};
