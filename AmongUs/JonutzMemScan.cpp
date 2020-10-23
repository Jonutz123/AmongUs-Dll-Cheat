#include "stdafx.h"
#include "JonutzMemScan.h"
#include <tlhelp32.h>
#include <map>
#include <string>
#include <comdef.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>

using std::map;
using std::string;

ModuleInfo _main_module;

ModuleInfo GetMainModule() {
	MODULEENTRY32 uModule;
	SecureZeroMemory(&uModule, sizeof(MODULEENTRY32));
	uModule.dwSize = sizeof(MODULEENTRY32);
	//Create snapshot of modules and Iterate them
	HANDLE hModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
	for (BOOL bModule = Module32First(hModuleSnapshot, &uModule); bModule; bModule = Module32Next(hModuleSnapshot, &uModule))
	{
		uModule.dwSize = sizeof(MODULEENTRY32);
		_bstr_t asd(uModule.szModule);
		const char * mdl = (const char *)asd;
		string tmp(mdl);
		std::vector<string> spl = Split(tmp, ".");
		if (spl.size() == 2) {
			if (spl[1] == "exe") {
				_main_module = { (DWORD)uModule.modBaseAddr,uModule.modBaseSize,tmp };
				return _main_module;
			}
		}
	}
}

ModuleInfo GetModule(std::string name) {
	MODULEENTRY32 uModule;
	SecureZeroMemory(&uModule, sizeof(MODULEENTRY32));
	uModule.dwSize = sizeof(MODULEENTRY32);
	//Create snapshot of modules and Iterate them
	HANDLE hModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
	for (BOOL bModule = Module32First(hModuleSnapshot, &uModule); bModule; bModule = Module32Next(hModuleSnapshot, &uModule))
	{
		uModule.dwSize = sizeof(MODULEENTRY32);
		_bstr_t asd(uModule.szModule);
		const char * mdl = (const char *)asd;
		string tmp(mdl);
		if (tmp==name){
			_main_module = { (DWORD)uModule.modBaseAddr,uModule.modBaseSize,tmp };
				return _main_module;
		}
	}
}

std::vector<int> findArray(std::vector<BYTE> bts) {
	DWORD start = _main_module.base;
	DWORD end = start + _main_module.size;
	int cindex = 0;
	int sz = bts.size();
	std::vector<int> v;
	MEMORY_BASIC_INFORMATION mbi;
	for (int i = start; i < end; i++) {
		if (bts[cindex] == *(BYTE*)(i)) {
			cindex++;
			if (cindex == sz) {
				v.push_back(i - sz + 1);
				cindex = 0;
			}
		}
		else {
			cindex = 0;
		}
	}
	return v;
}

std::vector<int> findArray(BYTE bts[]) {
	int sz = sizeof(bts);
	std::vector<BYTE> b;
	for (int i = 0; i < sz; i++) {
		b.push_back(bts[i]);
	}
	return findArray(b);
}

std::vector<int> findArrayPattern(std::vector<int> bts) {
	DWORD start = _main_module.base;
	DWORD end = start + _main_module.size;
	int cindex = 0;
	int sz = bts.size();
	std::vector<int> v;
	for (int i = start; i < end; i++) {
		if (bts[cindex] == 256 || bts[cindex] == *(BYTE*)(i)) {
			cindex++;
			if (cindex == sz) {
				v.push_back(i - sz + 1);
				cindex = 0;
			}
		}
		else {
			cindex = 0;
		}
	}
	return v;
}

std::vector<int> findArrayPattern(int bts[]) {
	int sz = sizeof(bts) / 4;
	std::vector<int> b;
	for (int i = 0; i < sz; i++) {
		b.push_back(bts[i]);
	}
	return findArrayPattern(b);
}

std::vector<string> Split(string s, string spl) {
	std::vector<string> out;
	string ax = "";
	string ax2 = "";
	int c = 0;
	for (int i = 0; i < s.size(); i++) {
		if (s[i] == spl[c]) {
			ax2 += spl[c];
			c++;
			if (c == spl.size()) {
				out.push_back(ax);
				ax = "";
				ax2 = "";
			}
		}
		else {
			ax += ax2;
			c = 0;
			ax += s[i];
		}
	}
	if (ax != "")
		out.push_back(ax);
	return out;
}

int getNumber(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	else if (c >= 'a' && c <= 'f') {
		return c - 'a'+10;
	}
	else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	else return -1;
}

std::vector<int> createPatternArray(std::string str) {
	std::vector<std::string> spl = Split(str, " ");
	std::vector<int> out;
	for (int i = 0; i < spl.size(); i++) {
		if (spl[i] == "256") {
			out.push_back(256);
		}
		else {
			if(spl[i].size()==2)
				out.push_back(getNumber(spl[i][0])*16+getNumber(spl[i][1]));
		}
	}
	return out;
}