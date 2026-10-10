
#include "Precomp.h"
#include "Utils/Exception.h"
#include "Utils/Logger.h"
#include "Utils/CommandLine.h"
#include "GameApp.h"
#include "GameFolder.h"
#include "LauncherLine.h"
#include "OriginalCommandLine.h"
#include "Engine.h"
#include "UI/WidgetResourceData.h"
#include "UI/ErrorWindow/ErrorWindow.h"
#include "UI/Launcher/LauncherWindow.h"
#include "Utils/File.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <unistd.h>
#endif
#include "Utils/Logger.h"

int GameApp::main(Array<std::string> args)
{
	InitWidgetResources("dark");
	int exitcode = 0;
	std::string relaunchLine;

	try
	{
		CommandLine cmd(args);
		commandline = &cmd;

		// The original's command line, from the launcher that started us, and
		// the line back to it (OriginalCommandLine, LauncherLine).
		if (commandline->HasArg("", "--cmdline"))
			OriginalCommandLine::Get().Set(commandline->GetArg("", "--cmdline"));
		LauncherLine::Get().Hello();

		// Stream the engine log to stderr in headless mode. Without this the
		// log only surfaces inside the modal error window, which is no use on
		// a test rig or an embedded target -- and LogUnimplemented() is exactly
		// how we find out which natives a game still needs.
		if (commandline->HasArg("-n", "--no-launcher") || commandline->HasArg("-v", "--verbose"))
		{
			Logger::Get()->SetCallback([](const LogMessageLine& line)
			{
				std::cerr << "[" << line.Source << "] " << line.Text << std::endl;
			});
		}

		if (ErrorWindow::CheckCrashReporter())
			return 0;

		if (commandline->HasArg("-h", "--help"))
		{
			std::cout << "SurrealEngine [--url=<mapname>] [--engineversion=X] [--server [--lanplay]] [--cmdline=<the original's>] [Path to game folder]\n";
			return 0;
		}

		// Skip the desktop launcher window when a game folder is given on the
		// command line, or when --no-launcher is passed.
		//
		// Needed twice over: it is the only way to drive the engine headlessly
		// for testing, and the handheld target has no mouse and no desktop --
		// game selection happens in our own launcher before this process starts.
		int selectedGameIndex;
		if (!commandline->GetItems().empty() || commandline->HasArg("-n", "--no-launcher"))
		{
			GameFolderSelection::UpdateList();
			if (GameFolderSelection::Games.empty())
			{
				std::cout << "No UE1 game found in the given folder." << std::endl;
				selectedGameIndex = -1;
			}
			else
			{
				selectedGameIndex = 0;
				std::cout << "Launching " << GameFolderSelection::Games[0].gameName
				          << " " << GameFolderSelection::Games[0].gameVersionString
				          << std::endl;
			}
		}
		else
		{
			selectedGameIndex = LauncherWindow::ExecModal();
		}
		if (selectedGameIndex >= 0)
		{
			GameLaunchInfo info = GameFolderSelection::GetLaunchInfo(selectedGameIndex);
			Engine engine(info);
			engine.Run();
			relaunchLine = engine.RelaunchLine;
		}
	}
	catch (const std::exception& e)
	{
		// Always report to stderr as well as the GUI. On a headless test rig or
		// an embedded target there may be no window to read the message in, and
		// a modal error window with nobody to close it just spins.
		exitcode = 1;
		std::cerr << "SurrealEngine error: " << e.what() << std::endl;
		for (const LogMessageLine& line : Logger::Get()->GetLog())
			std::cerr << "  log: " << line.Text << std::endl;
		if (!commandline || !commandline->HasArg("-n", "--no-launcher"))
			ErrorWindow::ExecModal(e.what(), Logger::Get()->GetLog());
	}

	DeinitWidgetResources();
	if (exitcode == 0 && !relaunchLine.empty())
		Relaunch(args, relaunchLine);
	return exitcode;
}

void GameApp::Relaunch(const Array<std::string>& args, const std::string& line)
{
	// A RELAUNCH with no launcher to start (Engine::Relaunch): this engine
	// again, with the same options but the original's command line the
	// RELAUNCH named in place of its start (--cmdline, --url, --server,
	// --lanplay, --timeline).
#ifndef _WIN32
	auto option = [](const std::string& arg, const char* name)
	{
		size_t n = std::strlen(name);
		return arg.compare(0, n, name) == 0 && (arg.size() == n || arg[n] == '=');
	};
	Array<std::string> next;
	for (const std::string& arg : args)
	{
		if (option(arg, "--cmdline") || option(arg, "--url") || option(arg, "-u") || option(arg, "--server") ||
			option(arg, "--lanplay") || option(arg, "--timeline"))
			continue;
		next.push_back(arg);
	}
	next.push_back("--cmdline=" + line);

	char path[4096];
	ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (length <= 0)
	{
		std::cerr << "Relaunch: the engine's own path is unknown" << std::endl;
		return;
	}
	path[length] = 0;
	std::vector<char*> argv;
	argv.push_back(path);
	for (std::string& arg : next)
		argv.push_back(arg.data());
	argv.push_back(nullptr);
	std::cerr << "Relaunch: " << path << " --cmdline=" << line << std::endl;
	execv(path, argv.data());
	std::cerr << "Relaunch: " << std::strerror(errno) << std::endl;
#else
	std::cerr << "Relaunch: not on this platform" << std::endl;
#endif
}
