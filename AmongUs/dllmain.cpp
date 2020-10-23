// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "SigScanDLL.h"
#include "AmongUs.h"
#include "detours.h"
#include "JonutzMemScan.h"
#include "JonutzMono.h"

/*
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/appdomain.h>
*/


#pragma comment(lib, "detours.lib")

using namespace std;
using namespace jmono;

ModuleInfo main_module;

DWORD LocalPlayer=0;
DWORD GameSettings = 0;
DWORD GameData = 0;

//Functions pointers
DWORD GetPlayerById = 0;
DWORD CmdCheckColor = 0;
DWORD PlayerControlInit = 0;
DWORD PlayerControlOnDestroy = 0;
DWORD GetData = 0;
DWORD KillPlayer = 0;
DWORD CmdChangeName = 0;
DWORD CmdReportBody = 0;
DWORD GameSettingsInit = 0;
DWORD CmdSetHat = 0;
DWORD CmdSetPet = 0;
DWORD CmdSetSkin = 0;
DWORD SetInfected = 0;

typedef int(*ExecuteVoid)(int instance);
typedef int(*ExecuteIntZero)(int instance);
typedef int(*ExecuteInt)(int instance,DWORD val);
typedef int(*ExecuteIntPtr)(int instance, DWORD * val);
typedef float(*ExecuteFloat)(int instance,float val);
typedef int(*PlayerControlStart)(int a1);
typedef int(*PlayerControlDestroy)(int a1);
typedef int(*PlayerColorChange)(int a1, char id);
typedef int(*PlayerNameChange)(int a1, int nmPt);
typedef int(*GetPlayerInfo)(int a1, char id);

/*
MonoDomain *domain;
MonoAssembly *assembly;
MonoImage* image;
*/

void writeInt(DWORD addr,int val) {
	*(int *)(addr) = val;
}

void writeFloat(DWORD addr, float val) {
	*(float *)(addr) = val;
}

void writeByte(DWORD addr, BYTE val) {
	*(BYTE *)(addr) = val;
}

void writeString(DWORD _addr,string str) {
	DWORD addr = _addr;
	 writeInt(addr + 8,str.size());
	for (int i = 1; i <= str.size(); i++) {
		writeByte(addr + 12 + (i - 1) * 2,str[i-1]);
	}
}

int readInt(DWORD addr) {
	return *(int *)(addr);
}

BYTE readByte(DWORD addr) {
	return *(BYTE *)(addr);
}

float readFloat(DWORD addr ) {
	return *(float *)(addr);
}

string readString(DWORD _addr) {
	DWORD addr = _addr;
	int ln = readInt(addr + 8);
	string str = "";
	for (int i = 1; i <= ln; i++) {
		BYTE b = readByte(addr + 12 + (i - 1) * 2);
		str += b;
	}
	return str;
}

void print(string msg) {
	fprintf(stdout, msg.c_str());
}

int sig_scan(std::string sig)
{
	InitializeSigScan(GetCurrentProcessId(), "GameAssembly.dll");
	int result = SigScan(sig.c_str(), 0);
	FinalizeSigScan();
	return result;
}

void InitializeJonutzScan() {
	main_module = GetModule("GameAssembly.dll");
}

struct Player {
	DWORD playerControl;
	int dataPt=0;

	BYTE getId() {
		return *(char *)(&playerControl+ OFF_playerId);
	}

	void getData() {
		ExecuteIntZero fnc = (ExecuteIntZero)GetData;
		dataPt = fnc(playerControl);
	}

	string getName() {
		if (dataPt == 0) return "data undefined";
		string str = readString(readInt(dataPt + OFF_playerName));
		return str;
	}

	bool isImpostor() {
		return (bool)readByte(dataPt + OFF_impostor);
	}

	bool isDead() {
		return (bool)readByte(dataPt + OFF_dead);
	}

	void setImpostor(bool v) {
		if (v) {
			writeByte(dataPt + OFF_impostor, 1);
		}
		else {
			writeByte(dataPt + OFF_impostor, 0);
		}
	}

	void setDead(bool v) {
		if (v) {
			writeByte(dataPt + OFF_dead, 1);
		}
		else {
			writeByte(dataPt + OFF_dead, 0);
		}
	}

	void setColor(BYTE id) {
		PlayerColorChange fnc = (PlayerColorChange)CmdCheckColor;
		fnc(playerControl,id);
	}

	void setHat(DWORD id) {
		ExecuteInt fnc = (ExecuteInt)CmdSetHat;
		fnc(playerControl, id);
	}

	void setPet(DWORD id) {
		ExecuteInt fnc = (ExecuteInt)CmdSetPet;
		fnc(playerControl, id);
	}

	void setSkin(DWORD id) {
		ExecuteInt fnc = (ExecuteInt)CmdSetSkin;
		fnc(playerControl, id);
	}

	void setKillTimer(float val) {
		writeFloat(dataPt + OFF_killTimer, val);
	}

	void kill(DWORD target) {
		ExecuteInt fnc = (ExecuteInt)KillPlayer;
		print(std::to_string(target)+"\n");
		fnc(playerControl, target);
	}

	void report(Player target) {
		ExecuteInt fnc = (ExecuteInt)CmdReportBody;
		fnc(playerControl, target.dataPt);
	}

	void setName(string newname) {
		int pt = readInt(dataPt + OFF_playerName);
		writeString(pt, newname);
	}

	void changeName(string newname) {
		int pt = readInt(dataPt + OFF_playerName);
		writeString(pt, newname);
		PlayerNameChange fnc = (PlayerNameChange)CmdChangeName;
		fnc(playerControl, pt);
	}
};

vector <Player> players;
vector <string> commands;

struct GameVar {
	string name;
	int off;
	int type;
};
vector<GameVar> vars;
string myName = "";
DWORD localPlayer;
Player* LocalPlayerPtr;

#pragma region Hooks
int HookPlayerControlStart(int a1)
{
	//print("NEW PLAYER CONTROL " + to_string(a1) + "\n");
	Player newPlayer;
	newPlayer.playerControl = a1;
	newPlayer.getData();
	string nm=newPlayer.getName();
	if (nm == myName) {
		localPlayer = a1;
		LocalPlayerPtr = &newPlayer;
		print("Local player at " + to_string(a1) + "\n");
	}
	players.push_back(newPlayer);
	//print("New player connected [" + nm +","+ to_string(newPlayer.playerControl)+"]\n");
	PlayerControlStart OriginalPlayerControlStart = (PlayerControlStart)PlayerControlInit;
	return OriginalPlayerControlStart(a1);
}

int HookPlayerControlOnDestroy(int a1)
{
	//print("PLAYER CONTROL DESTROY " + to_string(a1) + "\n");
	for (int i = 0; i < players.size(); i++) {
		if (players[i].playerControl == a1) {
			//print("Player disconnected [" + players[i].getName() + "]\n");
			players.erase(players.begin() + i);
			break;
		}
	}
	PlayerControlDestroy OriginalPlayerControlDestroy = (PlayerControlDestroy)PlayerControlOnDestroy;
	return OriginalPlayerControlDestroy(a1);
}

int HookCmdCheckColor(int a1, char id)
{
	print("COLOR " + to_string(a1) + " " + to_string((int)id) + "\n");
	PlayerColorChange OriginalCmdCheckColor = (PlayerColorChange)CmdCheckColor;
	return OriginalCmdCheckColor(a1, id);
}

int HookCmdChangeName(int a1, DWORD nmPt)
{
	print("NEW NAME " + to_string(a1) + " " + to_string((int)nmPt) + "\n");
	//print("NAME "+readString(nmPt)+"\n");
	ExecuteInt OriginalCmdChangeName = (ExecuteInt)CmdChangeName;
	return OriginalCmdChangeName(a1, nmPt);
}

int HookKillPlayer(int a1, int nmPt)
{
	//print("KILL PLAYER " + to_string(a1) + " " +" "+to_string(0)+" "+to_string(0) + "\n");
	ExecuteInt OriginalKill = (ExecuteInt)KillPlayer;
	return OriginalKill(a1, nmPt);
}

int HookCmdReportBody(int a1, int nmPt)
{
	print("REPORT BODY " + to_string(a1) + " " + to_string((int)nmPt) + "\n");
	ExecuteInt OriginalReport = (ExecuteInt)CmdReportBody;
	return OriginalReport(a1, nmPt);
}

int HookGameSettingsInit(int a1,int xx)
{
	GameSettings = a1;
	ExecuteInt OriginalGameSettings = (ExecuteInt)GameSettingsInit;
	return OriginalGameSettings(a1,xx);
}

int HookGetPlayerById(int a1, char id)
{
	//print("PLAYER ID " + to_string(a1) + " " + to_string((int)id) + "\n");
	GetPlayerInfo original_get_player_by_id = (GetPlayerInfo)GetPlayerById;
	DWORD last_a1 = a1;
	GameData = a1;
	int result = original_get_player_by_id(a1, id);
	//DWORD player_array = *(DWORD *)(a1 + 0x24);  // Offset is 0x24.
	//print("PLAYER ARRAY: " + to_string(player_array) + "\n");
	return result;
}
#pragma endregion

bool InitializeFunctions() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	/*
	//Initialize mono
	mono_set_dirs("C:\\Program Files (x86)\\Mono\\lib", "C:\\Program Files (x86)\\etc");
	print("LDF\n");
		domain = mono_jit_init("GameAssembly.dll"); //Assembly-CSharp.dll
		if (!domain) {
			print("Couldn't init mono domain\n");
			return false;
		}
		assembly = mono_domain_assembly_open(domain, "Assembly-CSharp.dll");
		image = mono_assembly_get_image(assembly);

		MonoClass* playerctrl = mono_class_from_name(image, "", "PlayerControl");
		print(to_string(((DWORD)playerctrl)));
	*/
	GetPlayerById = sig_scan(OFF_GetPlayerById);
	if (GetPlayerById == 0) {
		print("ERROR: GetPlayerById offset is 0\n");
	}
	else {
		print("GetPlayerById offset is " + to_string(GetPlayerById) + "\n");
	}
	CmdCheckColor = sig_scan(OFF_CmdCheckColor);
	if (CmdCheckColor == 0) {
		print("ERROR: CmdCheckColor offset is 0\n");
	}
	else {
		print("CmdCheckColor offset is " + to_string(CmdCheckColor) + "\n");
	}
	PlayerControlInit = sig_scan(OFF_PlayerControlStart);
	if (PlayerControlInit == 0) {
		print("ERROR: PlayerControlInit offset is 0\n");
	}
	else {
		print("PlayerControlInit offset is " + to_string(PlayerControlInit) + "\n");
	}
	PlayerControlOnDestroy = sig_scan(OFF_PlayerControlDestroy);
	if (PlayerControlOnDestroy == 0) {
		print("ERROR: PlayerControlOnDestroy offset is 0\n");
	}
	else {
		print("PlayerControlInit offset is " + to_string(PlayerControlOnDestroy) + "\n");
	}
	GetData = sig_scan(OFF_GetData);
	if (GetData == 0) {
		print("ERROR: GetData offset is 0\n");
	}
	else {
		print("GetData offset is " + to_string(GetData) + "\n");
	}
	KillPlayer = sig_scan(OFF_killPlayer);
	if (KillPlayer == 0) {
		print("ERROR: KillPlayer offset is 0\n");
	}
	else {
		print("KillPlayer offset is " + to_string(KillPlayer) + "\n");
	}
	CmdChangeName = sig_scan(OFF_CmdChangeName);
	if (CmdChangeName == 0) {
		print("ERROR: CmdChangeName offset is 0\n");
	}
	else {
		print("CmdChangeName offset is " + to_string(CmdChangeName) + "\n");
	}
	CmdReportBody = sig_scan(OFF_CmdReportBody);
	if (CmdChangeName == 0) {
		print("ERROR: CmdReportBody offset is 0\n");
	}
	else {
		print("CmdReportBody offset is " + to_string(CmdReportBody) + "\n");
	}
	GameSettingsInit = sig_scan(OFF_GameSettingsStart);
	if (GameSettingsInit == 0) {
		print("ERROR: GameSettingsInit offset is 0\n");
	}
	else {
		print("GameSettingsInit offset is " + to_string(GameSettingsInit) + "\n");
	}
	CmdSetHat = sig_scan(OFF_SetHat);
	if (CmdSetHat == 0) {
		print("ERROR: CmdSetHat offset is 0\n");
	}
	else {
		print("CmdSetHat offset is " + to_string(CmdSetHat) + "\n");
	}
	CmdSetPet = sig_scan(OFF_SetPet);
	if (CmdSetPet == 0) {
		print("ERROR: CmdSetPet offset is 0\n");
	}
	else {
		print("CmdSetPet offset is " + to_string(CmdSetPet) + "\n");
	}
	CmdSetSkin = sig_scan(OFF_SetSkin);
	if (CmdSetPet == 0) {
		print("ERROR: CmdSetSkin offset is 0\n");
	}
	else {
		print("CmdSetSkin offset is " + to_string(CmdSetSkin) + "\n");
	}
	/*
	vector<int> patterns;
	patterns = findArrayPattern(createPatternArray(OFF_SetInfected));
	if (patterns.size() > 0) {
		SetInfected = patterns[0];
	}
	else {
		print("SetInfected patterns found: " + to_string(patterns.size())+"\n");
	}
	*/
	SetInfected = main_module.base + OFF_SetInfected;
	if (SetInfected == 0) {
		print("ERROR: SetInfected offset is 0\n");
	}
	else {
		print("SetInfected offset is " + to_string(SetInfected) + "\n");
	}
	if(GetPlayerById!=0)
		DetourAttach(&(LPVOID&)GetPlayerById, &HookGetPlayerById);
	//if(CmdCheckColor!=0)
	//DetourAttach(&(LPVOID&)CmdCheckColor, &HookCmdCheckColor);
	if(PlayerControlInit!=0)
		DetourAttach(&(LPVOID&)PlayerControlInit, &HookPlayerControlStart);
	if(PlayerControlOnDestroy!=0)
		DetourAttach(&(LPVOID&)PlayerControlOnDestroy, &HookPlayerControlOnDestroy);
	/*
	if(CmdChangeName!=0)
		DetourAttach(&(LPVOID&)CmdChangeName, &HookCmdChangeName);
	if(KillPlayer!=0)
		DetourAttach(&(LPVOID&)KillPlayer, &HookKillPlayer);
	*/
	//DetourAttach(&(LPVOID&)CmdReportBody, &HookCmdReportBody);
	if(GameSettingsInit!=0)
		DetourAttach(&(LPVOID&)GameSettingsInit, &HookGameSettingsInit);
	DetourTransactionCommit();

	commands.push_back("impostor [player name or me]");
	commands.push_back("noimpostor [player name or me]");
	commands.push_back("dead [player name or me]");
	commands.push_back("revive [player name or me]");
	commands.push_back("colorloop [time in seconds]");
	commands.push_back("kill [player name or me]");
	commands.push_back("impostors");
	commands.push_back("help");

	vars.push_back({"maxplayers",OFF_MaxPlayers ,0});
	vars.push_back({"mapid", OFF_MapId,0});
	vars.push_back({"speed", OFF_Speed,2});
	vars.push_back({"crewvision", OFF_CrewVision,2});
	vars.push_back({"impostorvision", OFF_ImpostorVision,2});
	vars.push_back({"killcooldown", OFF_KillCooldown,2});
	vars.push_back({"commontasks", OFF_CommonTasks,0});
	vars.push_back({"longtasks", OFF_LongTasks,0});
	vars.push_back({"shortasks", OFF_ShortTasks,0});
	vars.push_back({"emergency", OFF_Emergency,0});
	vars.push_back({"emergencycooldown", OFF_EmergencyCooldown,2});
	vars.push_back({"impostors", OFF_Impostors,0});
	vars.push_back({"killdistance", OFF_KillDistance,0});
	vars.push_back({"discussiontime", OFF_DiscussionTime,0});
	vars.push_back({"votingtime", OFF_VotingTime,0 });
	vars.push_back({"confirmimpostor", OFF_ConfirmImpostor,1});
	vars.push_back({"visualtasks", OFF_VisualTasks,1});
	return true;
}

bool on = true;

void Close() {
	on = false;
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	if (SetInfected != 0) {
		DWORD useless=0;
		VirtualProtect((void*)(SetInfected + OFF_DeleteImpostors), 1, PAGE_EXECUTE_READWRITE, &useless);
		writeByte(SetInfected + OFF_DeleteImpostors, 1);
	}
	if (GetPlayerById != 0)
		DetourDetach(&(LPVOID&)GetPlayerById, &HookGetPlayerById);
	if (CmdCheckColor != 0)
		DetourDetach(&(LPVOID&)CmdCheckColor, &HookCmdCheckColor);
	if (PlayerControlInit != 0)
		DetourDetach(&(LPVOID&)PlayerControlInit, &HookPlayerControlStart);
	if (PlayerControlOnDestroy != 0)
		DetourDetach(&(LPVOID&)PlayerControlOnDestroy, &HookPlayerControlOnDestroy);
	/*
	if (KillPlayer != 0)
		DetourDetach(&(LPVOID&)KillPlayer, &HookKillPlayer);
	if (CmdChangeName != 0)
		DetourDetach(&(LPVOID&)CmdChangeName, &HookCmdChangeName);
	*/
	//DetourAttach(&(LPVOID&)CmdReportBody, &HookCmdReportBody);
	if (GameSettingsInit != 0)
		DetourDetach(&(LPVOID&)GameSettingsInit, &HookGameSettingsInit);
	DetourTransactionCommit();
}

void ExecuteCommand(string cmd) {
	vector<string> args = Split(cmd, " ");
	if (args.size() < 1) return;
	if (cmd == "exit") {
		Close();
		return;
	}
	else if (args[0] == "sigscan") {
		if (args.size() == 2) {
			cout << "Scanning for:" << endl << "[" + args[1] + "]" << endl;
			DWORD addr = sig_scan(args[1]);
			cout << "Address: " << hex << addr << endl;
		}
		else {
			cout << "Invalid arguments" << endl;
		}
	}
	else if (args[0] == "players") {
		print("Players (" + to_string(players.size()) + "):\n");
		for (int i = 0; i < players.size(); i++) {
			print(players[i].getName() + "\n");
		}
	}
	else if (args[0] == "me") {
		Player* me;
		for (int i = 0; i < players.size(); i++) {
			if (players[i].getName() == myName) {
				me = &players[i];
				break;
			}
		}
		if (!me)
		{
			print("ERROR: unknown local player\n");
			return;
		}
		print("Me: adr(" + to_string(me->playerControl) + ")" + "\n");
	}
	else if (args[0] == "colorloop") {
		if (args.size() < 2)
		{
			print("Invalid arguments\n");
			return;
		}
		bool all = false;
		if (args.size() == 3) {
			if (args[2] == "all")
				all = true;
		}
		if (all == false) {
			Player* me;
			for (int i = 0; i < players.size(); i++) {
				if (players[i].getName() == myName) {
					me = &players[i];
					break;
				}
			}
			int tm = atoi(args[1].c_str());
			for (int i = 1; i <= tm * 10; i++)
			{
				me->setColor(i % 12);
				Sleep(120);
			}
		}
		else {
			int tm = atoi(args[1].c_str());
			for (int j = 1; j <= tm * 10; j++)
			{
				for (int i = 0; i < players.size(); i++) {
						players[i].setColor((i*j+j)%12);
						Sleep(60);
				}
				Sleep(100);
			}
		}
	}
	else if (args[0] == "hatloop") {
		if (args.size() < 2)
		{
			print("Invalid arguments\n");
			return;
		}
		bool all = false;
		if (args.size() == 3) {
			if (args[2] == "all")
				all = true;
		}
		if (all == false) {
			Player* me;
			for (int i = 0; i < players.size(); i++) {
				if (players[i].getName() == myName) {
					me = &players[i];
					break;
				}
			}
			int tm = atoi(args[1].c_str());
			for (int i = 1; i <= tm * 10; i++)
			{
				me->setHat(i*i % 74);
				Sleep(100);
			}
		}
		else {
			int tm = atoi(args[1].c_str());
			for (int j = 1; j <= tm * 10; j++)
			{
				for (int i = 0; i < players.size(); i++) {
						players[i].setHat((i*j*4 + j) % 74);
						Sleep(20);
				}
				Sleep(100);
			}
		}
	}
	else if (args[0] == "allloop") {
		if (args.size() < 2)
		{
			print("Invalid arguments\n");
			return;
		}
		bool all = false;
		if (args.size() == 3) {
			if (args[2] == "all")
				all = true;
		}
		if (all == false) {
			Player* me;
			for (int i = 0; i < players.size(); i++) {
				if (players[i].getName() == myName) {
					me = &players[i];
					break;
				}
			}
			int tm = atoi(args[1].c_str());
			for (int i = 1; i <= tm * 10; i++)
			{
				if(i%4==0)
				me->setHat(i*i % 74);
				if (i % 4 == 1)
				me->setColor(i % 12);
				if (i % 4 == 2)
				me->setSkin(i % 16);
				if (i % 4 == 3)
				me->setPet(i % 11);
				Sleep(200);
			}
		}
		else {
			int tm = atoi(args[1].c_str());
			for (int j = 1; j <= tm * 10; j++)
			{
				for (int i = 0; i < players.size(); i++) {
						if ((i*j+i/2+rand()%5) % 4 == 0)
						players[i].setHat((i*j * 4 + j) % 74);
						if ((i*j+i/2+rand()%5) % 4 == 1)
						players[i].setColor(i % 12);
						if ((i*j+i/2+rand()%5) % 4 == 2)
						players[i].setSkin(i % 16);
						if ((i*j+i/2+rand()%5) % 4 == 3)
						players[i].setPet(i % 11);
						Sleep(40);
				}
				Sleep(100);
			}
		}
	}
	else if (args[0] == "impostor") {
		string plrname = "";
		if (args.size() < 2)
		{
			plrname = myName;
		}
		else {
			if (args[1] == "me") {
				plrname = myName;
			}
			else {
				for (int i = 1; i < args.size(); i++) {
					plrname += args[i];
					if (i < args.size() - 1) {
						plrname += " ";
					}
				}
			}
		}
		Player* player;
		for (int i = 0; i < players.size(); i++) {
			if (players[i].getName() == plrname) {
				player = &players[i];
				break;
			}
		}
		if (!player) return;
		player->setImpostor(true);
	}
	else if (args[0] == "noimpostor") {
		string plrname = "";
		if (args.size() < 2)
		{
			plrname = myName;
		}
		else {
			if (args[1] == "me") {
				plrname = myName;
			}
			else {
				for (int i = 1; i < args.size(); i++) {
					plrname += args[i];
					if (i < args.size() - 1) {
						plrname += " ";
					}
				}
			}
		}
		Player* player;
		for (int i = 0; i < players.size(); i++) {
			if (players[i].getName() == plrname) {
				player = &players[i];
				break;
			}
		}
		if (!player) return;
		player->setImpostor(false);
	}
	else if (args[0] == "revive") {
		string plrname = "";
		if (args.size() < 2)
		{
			plrname = myName;
		}
		else {
			if (args[1] == "me") {
				plrname = myName;
			}
			else {
				for (int i = 1; i < args.size(); i++) {
					plrname += args[i];
					if (i < args.size() - 1) {
						plrname += " ";
					}
				}
			}
		}
		Player* player;
		for (int i = 0; i < players.size(); i++) {
			if (players[i].getName() == plrname) {
				player = &players[i];
				break;
			}
		}
		if (!player) return;
		player->setDead(false);
	}
	else if (args[0] == "dead") {
		string plrname = "";
		if (args.size() < 2)
		{
			plrname = myName;
		}
		else {
			if (args[1] == "me") {
				plrname = myName;
			}
			else {
				for (int i = 1; i < args.size(); i++) {
					plrname += args[i];
					if (i < args.size() - 1) {
						plrname += " ";
					}
				}
			}
		}
		Player* player;
		for (int i = 0; i < players.size(); i++) {
			if (players[i].getName() == plrname) {
				player = &players[i];
				break;
			}
		}
		if (!player) return;
		player->setDead(true);
	}
	else if (args[0] == "impostors") {
		print("Impostors:\n==============================\n");
		for (int i = 0; i < players.size(); i++) {
			if (players[i].isImpostor()) {
				print(" * " + players[i].getName() + "\n");
			}
			Sleep(25);
		}
		print("==============================\n");
	}
	else if (args[0] == "kill") {
		string plrname = "";
		if (args.size() < 2)
		{
			plrname = myName;
		}
		else {
			if (args[1] == "me") {
				plrname = myName;
			}
			else {
				for (int i = 1; i < args.size(); i++) {
					plrname += args[i];
					if (i < args.size() - 1) {
						plrname += " ";
					}
				}
			}
		}
		Player * player;
		Player* me;
		for (int i = 0; i < players.size(); i++) {
			string nm = players[i].getName();
			if (nm == plrname) {
				player = &players[i];
			}
			if (nm == myName) {
				me = &players[i];
			}
		}
		print("Tryed to kill " + player->getName() + "\n");
		//me->kill(player->playerControl);
		ExecuteInt ff = (ExecuteInt)KillPlayer;
		ff(me->playerControl, (player->playerControl));
	}
	else if (args[0] == "reporttroll") {
		string plrname = "";
		if (args.size() < 2)
		{
			plrname = myName;
		}
		else {
			if (args[1] == "me") {
				plrname = myName;
			}
			else {
				for (int i = 1; i < args.size(); i++) {
					plrname += args[i];
					if (i < args.size() - 1) {
						plrname += " ";
					}
				}
			}
		}
		Player player;
		Player* me;
		for (int i = 0; i < players.size(); i++) {
			string nm = players[i].getName();
			if (nm == plrname) {
				player = players[i];
			}
			else if (nm == myName) {
				me = &players[i];
			}
		}
		if (player.playerControl != 0) return;
		me->report(player);
	}
	else if (cmd == "help") {
		print("Commands:\n==============================\n");
		for (int i = 0; i < commands.size(); i++) {
			print(commands[i] + "\n");
		}
		print("==============================\n");
	}
	else if (cmd == "deleteimpostors") {
		DWORD useless = 0;
		VirtualProtect((void*)(SetInfected + OFF_DeleteImpostors), 1, PAGE_EXECUTE_READWRITE, &useless);
		writeByte(SetInfected + OFF_DeleteImpostors, 0);
	}
	else if (cmd == "redoimpostors") {
		DWORD useless = 0;
		VirtualProtect((void*)(SetInfected + OFF_DeleteImpostors), 1, PAGE_EXECUTE_READWRITE, &useless);
		writeByte(SetInfected + OFF_DeleteImpostors, 1);
	}
	else if (args[0] == "setvar") {
		if (GameSettings == 0) {
			print("ERROR: game data not found\n");
			return;
		}
		if (args.size() != 3) {
			print("Invalid arguments setvar [var name] value\n");
			return;
		}
		transform(args[1].begin(), args[1].end(), args[1].begin(),
			[](unsigned char c) { return std::tolower(c); });
		GameVar vr;
		bool f = false;
		for (int i = 0; i < vars.size(); i++) {
			if (vars[i].name == args[1]) {
				f = true;
				vr = vars[i];
				break;
			}
		}
		if (!f) {
			print("Unknown var\n");
			return;
		}
		if (vr.type == 0) {
			int val = atoi(args[2].c_str());
			writeInt(GameSettings + vr.off, val);
		}
		else if (vr.type == 1) {
			char val = 0;
			if (args[2] == "true") val = 1;
			else val = 0;
			writeByte(GameSettings + vr.off, val);
		}
		else if (vr.type == 2) {
			float val = 0;
			val = stof(args[2]);
			writeFloat(GameSettings + vr.off, val);
		}
	}
	else if (args[0] == "setname") {
		string nm = "";
		for (int i = 1; i < args.size(); i++) {
			nm += args[i];
			if (i < args.size() - 1) {
				nm += " ";
			}
		}
		for (int i = 0; i < players.size(); i++) {
			string nm = players[i].getName();
			if (nm == myName) {
				LocalPlayerPtr = &players[i];
			}
		}
		if (nm != "")
		{
			myName = nm;
			//LocalPlayerPtr->setName(nm);
		}
		else
			print("Local player or name is null\n");
	}
	else if (cmd == "gamedata") {
		print(to_string(GameData) + "\n");
	}
	else if (cmd == "localplayer") {
		for (int i = 0; i < players.size(); i++) {
			string nm = players[i].getName();
			if (nm == myName) {
				print(to_string(players[i].playerControl) + "\n");
				break;
			}
		}
	}
	else {
		// print("Unknown command, try help\n");
	}
}

DWORD WINAPI Main(LPVOID param) {
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CONOUT$", "w", stdout);
	freopen("CONIN$", "r", stdin);
	print("Hooking...\n");
	InitializeJonutzScan();
	print("Jonutz memory scan loaded\n");
	InitializeFunctions();
	print("\nInjected\n");
	print("Enter your game nickname: ");
	getline(cin, myName);
	print("Welcome " + myName + "\nType help for commands!\n");
	while (on) {
		if (GetAsyncKeyState(VK_ESCAPE)) break;
		Sleep(50);
		print("> ");
		string cmd="";
		getline(cin, cmd);
		mono_command_executor(cmd);
		ExecuteCommand(cmd);
	}
	print("Closing...\n");
	print("Close the console window!\n");
	FreeConsole();
	FreeLibraryAndExitThread((HMODULE)param, 0);
	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, Main, hModule, 0, 0);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

