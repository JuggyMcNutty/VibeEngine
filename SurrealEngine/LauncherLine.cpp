#include "Precomp.h"
#include "LauncherLine.h"
#include "Utils/Logger.h"
#include <cstdlib>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

LauncherLine& LauncherLine::Get()
{
	static LauncherLine instance;
	return instance;
}

bool LauncherLine::Open()
{
	if (tried)
		return fd >= 0;
	tried = true;
#ifndef _WIN32
	const char* value = std::getenv("DXL_LAUNCHER_FD");
	if (!value || !*value)
		return false;
	char* end = nullptr;
	long n = std::strtol(value, &end, 10);
	if (*end || n < 0 || fcntl((int)n, F_GETFD) < 0)
	{
		LogMessage("Launcher line: no descriptor " + std::string(value));
		return false;
	}
	fd = (int)n;
	// Not handed on to anything this process starts.
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	return true;
#else
	return false;
#endif
}

void LauncherLine::Send(const std::string& line)
{
	if (!Open())
		return;
#ifndef _WIN32
	std::string data = line + "\n";
	if (send(fd, data.data(), data.size(), MSG_NOSIGNAL) < 0)
		LogMessage("Launcher line: the launcher is gone");
#endif
}

void LauncherLine::Hello()
{
	Send("hello");
}

void LauncherLine::Ready()
{
	Send("ready");
}

void LauncherLine::Poll(const std::function<void(const std::string&)>& onLine)
{
	if (!Open())
		return;
#ifndef _WIN32
	char buffer[1024];
	for (;;)
	{
		ssize_t n = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
		if (n <= 0)
		{
			if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
			{
				// The launcher has gone: nothing more will come.
				close(fd);
				fd = -1;
			}
			break;
		}
		pending.append(buffer, (size_t)n);
	}
	size_t end;
	while ((end = pending.find('\n')) != std::string::npos)
	{
		std::string line = pending.substr(0, end);
		pending.erase(0, end + 1);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
			onLine(line);
	}
#endif
}
