#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <vector>
#include <string>

using std::string;
using std::vector;

namespace jmono {

	void fill(BYTE val, DWORD s, DWORD e);

	int create_string(std::string val);

	int invoke(DWORD instance, DWORD function, std::string param);

	std::vector<string> utils_Split(string s, string spl);
	string utils_createFull(vector<string> &args, int start);

	void utils_print(string msg);

	string utils_hex(int val);

	void mono_command_executor(string command);
}