#include "stdafx.h"
#include "JonutzMono.h"
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
using std::vector;

namespace jmono {

	void fill(BYTE val, DWORD s,DWORD e){
		for (DWORD i = s; i < e; i++) {
			*((BYTE*)i) = val;
		}
	}

	int create_string(string val) {
		int reqsize = 4 + 4 + 4 + val.size() * 2;
		BYTE * alloc = new BYTE[reqsize];
		fill(255, (DWORD)alloc, (DWORD)alloc + 4);
		fill(0, (DWORD)alloc+4, (DWORD)alloc + 12);
		*((DWORD*)((DWORD)alloc + 8)) = val.size();
		DWORD base = (DWORD)alloc;
		for (int i = 1; i <= val.size(); i++) {
			*((BYTE*)(base +12 + (i-1) * 2)) = (BYTE)val[i-1];
			*((BYTE*)(base + 12 + (i - 1) * 2 + 1)) = 0;
		}
		return base;
	}

	typedef int(*ExecuteInt)(DWORD instance, DWORD ptr);

	int invoke(DWORD instance, DWORD function,DWORD point) {
		/*
		__asm {
			mov ebx, instance
			mov edi, point
			jmp function
		}
		*/
		ExecuteInt f = (ExecuteInt)function;
		return f(instance,point);
	}

	std::vector<string> utils_Split(string s, string spl) {
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

	string utils_createFull(vector<string> &args, int start) {
		string a = "";
		for (int i = start; i < args.size(); i++) {
			a += args[i];
			if (i < args.size() - 1) a += " ";
		}
		return a;
	}

	void utils_print(string msg) {
		fprintf(stdout, msg.c_str());
	}

	string utils_hex(int val) {
		std::stringstream stream;
		stream << std::hex << val;
		return stream.str();
	}
	
	void mono_command_executor(string command) {
		vector<string> args = utils_Split(command," ");
		if (args[0] == "create_string") {
			string val = utils_createFull(args, 1);
			int ret = create_string(val);
			utils_print(utils_hex(ret)+"\n");
		}
		else if (args[0] == "invoke_param_string") {
			if (args.size() <= 2) {
				return;
			}
			DWORD inst = stoi(args[1]);
			DWORD adr = stoi(args[2]);
			string val = utils_createFull(args, 3);
			int ret = create_string(val);
			invoke(inst, adr, ret);
		}
	}
}