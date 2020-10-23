#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>

struct ModuleInfo {
	DWORD base;
	DWORD size;
	std::string name;
};

ModuleInfo GetMainModule();
ModuleInfo GetModule(std::string name);
std::vector<int> findArray(std::vector<BYTE> bts);
std::vector<int> findArrayPattern(std::vector<int> bts);
std::vector<int> createPatternArray(std::string str);
std::vector<std::string> Split(std::string s, std::string spl);
