#include <windows.h>
#include <unordered_map>
#include "FLCoreCommon.h"
#include "FLCoreServer.h"

#define NAKED	__declspec(naked)
#define WIN32_LEAN_AND_MEAN

#define JUMP( from, to ) \
	*from = 0xE9;\
	*(DWORD*)(from+1) = (PBYTE)to - from - 5

#define VirtualProtectX( addr, size ) \
	VirtualProtect( addr, size, PAGE_EXECUTE_READWRITE, &dummy )

DWORD dummy;

struct SystemBackground
{
	float scale = 0.0f;
	Vector offset = { 0,0,0 };
};

std::unordered_map<uint, SystemBackground> SystemStructs;

void LoadConfigFile()
{
	SystemBackground settings;

	INI_Reader ini;
	if (!ini.open("../data/universe/backgrounds.ini", 1))
	{
		return;
	}

	while (ini.read_header())
	{
		if (ini.is_header("System"))
		{
			uint nickname;
			float systemSize = 1.0f;
			float modelSize = 0.0f;
			while (ini.read_value())
			{
				if (ini.is_value("system"))
				{
					nickname = CreateID(ini.get_value_string());
				}
				else if (ini.is_value("model_size"))
				{
					modelSize = ini.get_value_float(0);
				}
				else if (ini.is_value("system_size"))
				{
					systemSize = ini.get_value_float(0);
				}
				else if (ini.is_value("offset"))
				{
					settings.offset = { ini.get_value_float(0), ini.get_value_float(1), ini.get_value_float(2) };
				}
			}
			settings.scale = modelSize / systemSize;
			SystemStructs[nickname] = settings;
		}
	}
	ini.close();
}

Vector skyboxCamera;
void CalculateSkyboxCamera()
{
	static bool logicInit = false;
	if (!logicInit)
	{
		LoadConfigFile();
		logicInit = true;
	}

	static float scaleMod = 0.0f;
	static unsigned int currentSystem = 0;
	static Vector offset = { 0,0,0 };
	const unsigned int* SystemNickname = reinterpret_cast<unsigned int*>(0x673354);

	if (*SystemNickname != currentSystem)
	{
		currentSystem = *SystemNickname;
		std::unordered_map<uint, SystemBackground>::iterator systemData = SystemStructs.find(*SystemNickname);
		if(systemData != SystemStructs.end())
		{
			scaleMod = systemData->second.scale;
			offset = systemData->second.offset;
		}
		else
		{
			scaleMod = 0.f;
			offset = { 0,0,0 };
		}
	}

	const Vector* CameraCoords = reinterpret_cast<Vector*>(0x00667C5C);

	skyboxCamera.x = (offset.x + CameraCoords->x) * scaleMod;
	skyboxCamera.y = (offset.y + CameraCoords->y) * scaleMod;
	skyboxCamera.z = (offset.z + CameraCoords->z) * scaleMod;
}

constexpr int SkyboxFLMatReturn = 0x66FE614;
NAKED
void SkyboxCameraHook()
{
	__asm
	{
		pushad
		call CalculateSkyboxCamera
		popad

		push eax

		mov DWORD PTR[esp + 0x68 + 4], 00000000

		mov eax, skyboxCamera
		mov[esp + 0x24 + 4], eax

		mov eax, skyboxCamera + 4
		mov[esp + 0x28 + 4], eax

		mov eax, skyboxCamera + 8
		mov[esp + 0x2C + 4], eax

		pop eax
		jmp SkyboxFLMatReturn
	}
}

void Patch()
{
#define HookSkyFLMat ((PBYTE)0x066FE5F4)
#define HookSkyFLMat2 ((PBYTE)0x066FE5F9)
	VirtualProtectX(HookSkyFLMat, 8);
	JUMP(HookSkyFLMat, SkyboxCameraHook);
	memcpy(HookSkyFLMat2, "\x90\x90\x90", 3);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		Patch();

	return TRUE;
}
