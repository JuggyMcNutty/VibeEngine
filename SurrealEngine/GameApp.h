#pragma once

class GameApp
{
public:
	int main(Array<std::string> args);

private:
	static void Relaunch(const Array<std::string>& args, const std::string& line);
};
