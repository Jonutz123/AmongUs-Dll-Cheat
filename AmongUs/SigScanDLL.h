#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <vector>
#include <string>

#ifdef SIGSCANDLL_EXPORTS
#define SIGSCAN_API __declspec(dllexport)
#else
#define SIGSCAN_API __declspec(dllimport)
#endif


 DWORD SigScan(const char* szPattern, int offset = 0);
 void InitializeSigScan(DWORD ProcessID, const char* szModule);
 void FinalizeSigScan();

std::vector<std::string> Split(std::string s,std::string spl);
