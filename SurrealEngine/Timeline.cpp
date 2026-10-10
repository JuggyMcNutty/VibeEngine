
#include "Precomp.h"
#include "Timeline.h"
#include "Engine.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Extension/Windows/TabGroup/URootWindow.h"
#include "Utils/File.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include <sstream>

bool Timeline::Load(const std::string& path)
{
	std::string text;
	try
	{
		text = File::read_all_text(path);
	}
	catch (...)
	{
		LogMessage("Timeline: cannot read " + path);
		return false;
	}

	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line))
	{
		size_t hash = line.find('#');
		if (hash != std::string::npos)
			line.resize(hash);
		std::istringstream words(line);
		std::string clock;
		Action action;
		if (!(words >> clock >> action.Time >> action.Verb))
			continue;
		if (clock != "start" && clock != "game")
		{
			LogMessage("Timeline: no clock '" + clock + "' (start or game)");
			continue;
		}
		action.InGame = clock == "game";
		std::getline(words, action.Arg);
		size_t first = action.Arg.find_first_not_of(" \t\r");
		action.Arg = first == std::string::npos ? std::string() : action.Arg.substr(first);
		while (!action.Arg.empty() && (action.Arg.back() == '\r' || action.Arg.back() == ' '))
			action.Arg.pop_back();
		Actions.push_back(action);
	}
	LogMessage("Timeline: " + std::to_string(Actions.size()) + " actions from " + path);
	return true;
}

void Timeline::Tick(float realElapsed)
{
	StartTime += realElapsed;

	bool inGame = engine->LevelInfo && engine->LevelInfo->NetMode() == NM_Client && engine->viewport && engine->viewport->Actor();
	if (inGame)
	{
		GameTime += realElapsed;
		WasInGame = true;
	}
	else if (WasInGame)
	{
		LogMessage("DXLIVE: back in the menu after " + std::to_string((int)GameTime) + " s of game; exiting");
		WasInGame = false;
		uint32_t foundBits = 0;
		BitfieldBool found = { &foundBits, 1 };
		engine->ConsoleCommand(engine->viewport ? engine->viewport->Actor() : nullptr, "exit", found);
		return;
	}

	for (Action& action : Actions)
	{
		if (action.Done)
			continue;
		if (action.InGame ? (inGame && GameTime >= action.Time) : StartTime >= action.Time)
		{
			action.Done = true;
			Run(action);
		}
	}

	if (inGame && GameTime - LogTime >= 1.0f)
	{
		LogTime = GameTime;
		LogPlaces();
	}
}

void Timeline::Run(Action& action)
{
	if (action.Verb == "press" || action.Verb == "release")
	{
		for (int i = 0; i < 256; i++)
		{
			if (Engine::keynames[i] && StrTools::equals_ignore_case(Engine::keynames[i], action.Arg))
			{
				LogMessage("DXLIVE: " + action.Verb + " " + action.Arg);
				EInputKey key = (EInputKey)i;
				bool mouse = key == IK_LeftMouse || key == IK_RightMouse || key == IK_MiddleMouse;
				// A wheel notch is a press alone, as the window sends it.
				if (key == IK_MouseWheelUp || key == IK_MouseWheelDown)
				{
					if (action.Verb == "press")
						engine->OnWindowMouseWheel(Point(), key);
					return;
				}
				if (action.Verb == "press")
					mouse ? engine->OnWindowMouseDown(Point(), key) : engine->OnWindowKeyDown(key);
				else
					mouse ? engine->OnWindowMouseUp(Point(), key) : engine->OnWindowKeyUp(key);
				return;
			}
		}
		LogMessage("Timeline: no key '" + action.Arg + "'");
		return;
	}

	if (action.Verb == "pointer")
	{
		std::istringstream xy(action.Arg);
		float x = 0.0f, y = 0.0f;
		if (engine->dxRootWindow && (xy >> x >> y))
		{
			LogMessage("DXLIVE: pointer " + action.Arg);
			engine->dxRootWindow->SetRootCursorPos(x, y);
		}
		return;
	}

	std::string command = action.Arg.empty() ? action.Verb : action.Verb + " " + action.Arg;
	LogMessage("DXLIVE: " + command);
	uint32_t foundBits = 0;
	BitfieldBool found = { &foundBits, 1 };
	engine->ConsoleCommand(engine->viewport ? engine->viewport->Actor() : nullptr, command, found);
}

void Timeline::LogPlaces()
{
	UActor* player = engine->viewport->Actor();
	auto vec = [](const vec3& v) { return std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z); };
	std::string t = std::to_string((int)GameTime);
	LogMessage("DXLIVE: t=" + t + " at " + vec(player->Location()) + " state " + player->GetStateName().ToString() + " physics " + std::to_string(player->Physics()));
	for (UActor* actor : engine->Level->Actors)
	{
		UPawn* pawn = UObject::TryCast<UPawn>(actor);
		if (!pawn || actor == player)
			continue;
		LogMessage("DXLIVE: t=" + t + " other " + pawn->Name.ToString() + " at " + vec(pawn->Location()) + " velocity " + vec(pawn->Velocity()) +
			" anim " + pawn->AnimSequence().ToString() + " frame " + std::to_string(pawn->AnimFrame()) + " rate " + std::to_string(pawn->AnimRate()));
	}
}
