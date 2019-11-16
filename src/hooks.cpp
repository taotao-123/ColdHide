/*
	Copyright (c) 2019 Rat431 (https://github.com/Rat431).
	This software is under the MIT license, for more informations check the LICENSE file.
*/

#include "hooks.h"
#include "Defs.h"
#include <vector>
#include <versionhelpers.h>
#include <tlhelp32.h>

namespace Hooks_Informastion
{
	HMODULE hNtDll = NULL;
	HMODULE hKernel = NULL;
	HMODULE hKernel32 = NULL;
	void* KIUEDRPage = NULL;
	DWORD CurrentProcessID = NULL;
	DWORD FPPID = NULL;

	// PEB.
	void* PEB_BeingDebuggedP = NULL;
	int32_t PEB_BeingDebuggedID = NULL;

	void* PEB_NtGlobalFlagP = NULL;
	int32_t PEB_NtGlobalFlagID = NULL;

	// HeapFlags
	void* FlagsHeapFlagsP = NULL;
	int32_t FlagsHeapFlagsID = NULL;

	// Some ntdll apis
	void* Nt_QueryProcessP = NULL;
	int32_t Nt_QueryProcessID = NULL;

	void* Nt_QuerySystemP = NULL;
	int32_t Nt_QuerySystemID = NULL;

	void* Nt_SetThreadInformationP = NULL;
	int32_t Nt_SetThreadInformationID = NULL;

	void* Nt_CloseP = NULL;
	int32_t Nt_CloseID = NULL;

	void* Nt_QueryObjectP = NULL;
	int32_t Nt_QueryObjectID = NULL;

	void* Nt_NtGetContextThreadP = NULL;
	int32_t Nt_NtGetContextThreadID = NULL;

	void* Nt_NtSetContextThreadP = NULL;
	int32_t Nt_NtSetContextThreadID = NULL;

	void* Nt_ContinueP = NULL;
	int32_t Nt_ContinueID = NULL;

	void* Nt_CreateThreadExP = NULL;
	int32_t Nt_CreateThreadExID = NULL;

	void* Nt_ExceptionDispatcherP = NULL;
	int32_t Nt_ExceptionDispatcherID = NULL;

	void* Nt_SetInformationProcessP = NULL;
	int32_t Nt_SetInformationProcessID = NULL;

	void* Nt_YieldExecutionP = NULL;
	int32_t Nt_YieldExecutionID = NULL;

	void* Nt_SetDebugFilterStateP = NULL;
	int32_t Nt_SetDebugFilterStateID = NULL;

	void* Kernel32_Process32FirstWP = NULL;
	int32_t Kernel32_Process32FirstWID = NULL;

	void* Kernel32_Process32NextWP = NULL;
	int32_t Kernel32_Process32NextWID = NULL;
}

namespace Hooks_Config
{
	// Hide PEB.
	bool HideWholePEB = true;
	bool PEB_BeingDebugged = false;
	bool PEB_NtGlobalFlag = false;

	// HeapFlags
	bool HeapFlags = false;

	// DRx
	bool HideWholeDRx = true;
	bool FakeContextEmulation = false;

	bool DRx_ThreadContextRead = false;
	bool DRx_ThreadContextWrite = false;
	bool Nt_Continue = false;
	bool Nt_KiUserExceptionDispatcher = false;

	// Anti attach
	bool Anti_Anti_Attach = false;

	// Some ntdll apis
	bool Nt_QueryProcess = true;
	bool Nt_QuerySystem = true;
	bool Nt_SetThreadInformation = true;
	bool Nt_Close = true;
	bool Nt_QueryObject = true;
	bool Nt_CreateThreadEx = true;
	bool Nt_SetInformationProcess = true;
	bool Nt_YieldExecution = true;
	bool Nt_SetDebugFilterState = true;
	
	bool Kernel32_Process32First = true;
	bool Kernel32_Process32Next = true;
}

namespace Hooks
{
	static void HidePEB()
	{
		void* PEB_BaseAddress = NULL;
		void* _HeapAddress = NULL;

		// Get PEB Address by the offset, or you can even call NtQueryInformationProcess.
		PEB_BaseAddress = (void*)GetPebFunction();
		
		if (PEB_BaseAddress)
		{
			// Check if we should patch the flags first.
			if (Hooks_Config::PEB_BeingDebugged) {
				Hooks_Informastion::PEB_BeingDebuggedP = (void*)((ULONG_PTR)PEB_BaseAddress + PEB_BeingDebuggedOffset);
				*(BYTE*)Hooks_Informastion::PEB_BeingDebuggedP = 0;
			}
			if (Hooks_Config::PEB_NtGlobalFlag) {
				Hooks_Informastion::PEB_NtGlobalFlagP = (void*)((ULONG_PTR)PEB_BaseAddress + PEB_NtGlobalFlagOffset);
				*(BYTE*)Hooks_Informastion::PEB_NtGlobalFlagP = 0;
			}
			if (Hooks_Config::HeapFlags) {
				// Get Process heap base address
				std::memcpy(&_HeapAddress, (void*)((ULONG_PTR)PEB_BaseAddress + HeapPEB_Offset), MAX_ADDRESS_SIZE);

				if (_HeapAddress)
				{
					// Check if the current Windows OS is Windows Vista or greater as different force flags and heap flags offsets in older versions.
					if (IsWindowsVistaOrGreater())
					{
						// Check if HEAP_GROWABLE flag is not setted, if not, we set it
						if (*(DWORD*)((ULONG_PTR)_HeapAddress + HeapFlagsBaseWinHigher) & ~HEAP_GROWABLE) {
							*(DWORD*)((ULONG_PTR)_HeapAddress + HeapFlagsBaseWinHigher) |= HEAP_GROWABLE;
						}
						*(DWORD*)((ULONG_PTR)_HeapAddress + HeapForceFlagsBaseWinHigher) = 0;
					}
					else
					{
						// Check if HEAP_GROWABLE flag is not setted, if not, we set it
						if (*(DWORD*)((ULONG_PTR)_HeapAddress + HeapFlagsBaseWinLower) & ~HEAP_GROWABLE) {
							*(DWORD*)((ULONG_PTR)_HeapAddress + HeapFlagsBaseWinLower) |= HEAP_GROWABLE;
						}
						*(DWORD*)((ULONG_PTR)_HeapAddress + HeapForceFlagsBaseWinLower) = 0;
					}
				}
			}
		}
	}

	static void HideDRx()
	{
		Hooks_Informastion::Nt_NtGetContextThreadP = GetProcAddress(Hooks_Informastion::hNtDll, "NtGetContextThread");
		Hooks_Informastion::Nt_NtSetContextThreadP = GetProcAddress(Hooks_Informastion::hNtDll, "NtSetContextThread");

		if (Hooks_Informastion::Nt_NtGetContextThreadP)
		{
			if (Hooks_Config::DRx_ThreadContextRead)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_NtGetContextThreadID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_NtGetContextThreadP, Hook_emu::__NtGetContextThread, &ErrorCode);

				if (Hooks_Informastion::Nt_NtGetContextThreadID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_NtGetContextThreadID, &ErrorCode);
					Hooks_Informastion::Nt_NtGetContextThreadP = HookDataS.OriginalF;
				}
			}
		}
		if (Hooks_Informastion::Nt_NtSetContextThreadP)
		{
			if (Hooks_Config::DRx_ThreadContextWrite)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_NtSetContextThreadID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_NtSetContextThreadP, Hook_emu::__NtSetContextThread, &ErrorCode);

				if (Hooks_Informastion::Nt_NtSetContextThreadID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_NtSetContextThreadID, &ErrorCode);
					Hooks_Informastion::Nt_NtSetContextThreadP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideProcessInformations()
	{
		Hooks_Informastion::Nt_QueryProcessP = GetProcAddress(Hooks_Informastion::hNtDll, "NtQueryInformationProcess");

		if (Hooks_Informastion::Nt_QueryProcessP)
		{
			if (Hooks_Config::Nt_QueryProcess)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_QueryProcessID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_QueryProcessP, Hook_emu::__NtQueryInformationProcess, &ErrorCode);

				if (Hooks_Informastion::Nt_QueryProcessID > NULL && ErrorCode == NULL)
				{
					if (ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_QueryProcessID, &ErrorCode))
					{
						Hooks_Informastion::Nt_QueryProcessP = HookDataS.OriginalF;
					}
				}
			}
		}
	}
	static void HideSetInformationThread()
	{
		Hooks_Informastion::Nt_SetThreadInformationP = GetProcAddress(Hooks_Informastion::hNtDll, "NtSetInformationThread");

		if (Hooks_Informastion::Nt_SetThreadInformationP)
		{
			if (Hooks_Config::Nt_SetThreadInformation)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_SetThreadInformationID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_SetThreadInformationP, Hook_emu::__NtSetInformationThread, &ErrorCode);

				if (Hooks_Informastion::Nt_SetThreadInformationID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_SetThreadInformationID, &ErrorCode);
					Hooks_Informastion::Nt_SetThreadInformationP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideQuerySystemInformation()
	{
		Hooks_Informastion::Nt_QuerySystemP = GetProcAddress(Hooks_Informastion::hNtDll, "NtQuerySystemInformation");

		if (Hooks_Informastion::Nt_QuerySystemP)
		{
			if (Hooks_Config::Nt_QuerySystem)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_QuerySystemID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_QuerySystemP, Hook_emu::__NtQuerySystemInformation, &ErrorCode);

				if (Hooks_Informastion::Nt_QuerySystemID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_QuerySystemID, &ErrorCode);
					Hooks_Informastion::Nt_QuerySystemP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideCloseHandle()
	{
		Hooks_Informastion::Nt_CloseP = GetProcAddress(Hooks_Informastion::hNtDll, "NtClose");

		if (Hooks_Informastion::Nt_CloseP)
		{
			if (Hooks_Config::Nt_Close)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_CloseID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_CloseP, Hook_emu::__NtClose, &ErrorCode);

				if (Hooks_Informastion::Nt_CloseID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_CloseID, &ErrorCode);
					Hooks_Informastion::Nt_CloseP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideQueryObject()
	{
		Hooks_Informastion::Nt_QueryObjectP = GetProcAddress(Hooks_Informastion::hNtDll, "NtQueryObject");

		if (Hooks_Informastion::Nt_QueryObjectP)
		{
			if (Hooks_Config::Nt_QueryObject)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_QueryObjectID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_QueryObjectP, Hook_emu::__NtQueryObject, &ErrorCode);

				if (Hooks_Informastion::Nt_QueryObjectID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_QueryObjectID, &ErrorCode);
					Hooks_Informastion::Nt_QueryObjectP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideSetInformationProcess()
	{
		Hooks_Informastion::Nt_SetInformationProcessP = GetProcAddress(Hooks_Informastion::hNtDll, "NtSetInformationProcess");

		if (Hooks_Informastion::Nt_SetInformationProcessP)
		{
			if (Hooks_Config::Nt_SetInformationProcess)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_SetInformationProcessID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_SetInformationProcessP, Hook_emu::__NtSetInformationProcess, &ErrorCode);

				if (Hooks_Informastion::Nt_SetInformationProcessID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_SetInformationProcessID, &ErrorCode);
					Hooks_Informastion::Nt_SetInformationProcessP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideNtContinue()
	{
		Hooks_Informastion::Nt_ContinueP = GetProcAddress(Hooks_Informastion::hNtDll, "NtContinue");

		if (Hooks_Informastion::Nt_ContinueP)
		{
			if (Hooks_Config::Nt_Continue)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_ContinueID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_ContinueP, Hook_emu::__NtContinue, &ErrorCode);

				if (Hooks_Informastion::Nt_ContinueID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_ContinueID, &ErrorCode);
					Hooks_Informastion::Nt_ContinueP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideCreateThreadEx()
	{
		Hooks_Informastion::Nt_CreateThreadExP = GetProcAddress(Hooks_Informastion::hNtDll, "NtCreateThreadEx");

		if (Hooks_Informastion::Nt_CreateThreadExP)
		{
			if (Hooks_Config::Nt_CreateThreadEx)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_CreateThreadExID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_CreateThreadExP, Hook_emu::__NtCreateThreadEx, &ErrorCode);

				if (Hooks_Informastion::Nt_CreateThreadExID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_CreateThreadExID, &ErrorCode);
					Hooks_Informastion::Nt_CreateThreadExP = HookDataS.OriginalF;
				}
			}
		}
	}
	static BYTE x64KIUEDHook[25] = { 0x48, 0x89, 0xE1, 0x48, 0x81, 0xC1, 0xF0, 0x04, 0x00, 0x00, 0x48, 0x89,
		0xE2, 0xFF, 0x15, 0x0E, 0x00, 0x00, 0x00, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
	static void HideExceptionDispatcher()
	{
		Hooks_Informastion::Nt_ExceptionDispatcherP = GetProcAddress(Hooks_Informastion::hNtDll, "KiUserExceptionDispatcher");

		if (Hooks_Informastion::Nt_ExceptionDispatcherP)
		{
			if (Hooks_Config::Nt_KiUserExceptionDispatcher)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;
				void* TempPointer = NULL;
				bool IsRunning64Bit = ColdHook_Service::Is64BitProcess();

				if (IsRunning64Bit) {
					// Allocate a page 
					Hooks_Informastion::KIUEDRPage = VirtualAlloc(NULL, 0x100, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
					TempPointer = Hooks_Informastion::KIUEDRPage;
				}
				else {
					TempPointer = Hook_emu::__RKiUserExceptionDispatcher;
				}
				if (TempPointer)
				{
					Hooks_Informastion::Nt_ExceptionDispatcherID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
						Hooks_Informastion::Nt_ExceptionDispatcherP, TempPointer, &ErrorCode);

					if (Hooks_Informastion::Nt_ExceptionDispatcherID > NULL && ErrorCode == NULL)
					{
						ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_ExceptionDispatcherID, &ErrorCode);
						Hooks_Informastion::Nt_ExceptionDispatcherP = HookDataS.OriginalF;

						if (IsRunning64Bit) {
							std::memcpy(TempPointer, x64KIUEDHook, sizeof(x64KIUEDHook));
							*(void**)((ULONG_PTR)TempPointer + sizeof(x64KIUEDHook)) = Hooks_Informastion::Nt_ExceptionDispatcherP;
							*(void**)((ULONG_PTR)TempPointer + sizeof(x64KIUEDHook) + sizeof(void*)) = Hook_emu::__KiUserExceptionDispatcher;
						}
					}
				}
			}
		}
	}
	static void HideYieldExecution()
	{
		Hooks_Informastion::Nt_YieldExecutionP = GetProcAddress(Hooks_Informastion::hNtDll, "NtYieldExecution");

		if (Hooks_Informastion::Nt_YieldExecutionP)
		{
			if (Hooks_Config::Nt_YieldExecution)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_YieldExecutionID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_YieldExecutionP, Hook_emu::__NtYieldExecution, &ErrorCode);

				if (Hooks_Informastion::Nt_YieldExecutionID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_YieldExecutionID, &ErrorCode);
					Hooks_Informastion::Nt_YieldExecutionP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideSetDebugFilterState()
	{
		Hooks_Informastion::Nt_SetDebugFilterStateP = GetProcAddress(Hooks_Informastion::hNtDll, "NtSetDebugFilterState");

		if (Hooks_Informastion::Nt_SetDebugFilterStateP)
		{
			if (Hooks_Config::Nt_SetDebugFilterState)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Nt_SetDebugFilterStateID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Nt_SetDebugFilterStateP, Hook_emu::__NtSetDebugFilterState, &ErrorCode);

				if (Hooks_Informastion::Nt_SetDebugFilterStateID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Nt_SetDebugFilterStateID, &ErrorCode);
					Hooks_Informastion::Nt_SetDebugFilterStateP = HookDataS.OriginalF;
				}
			}
		}
	}
	static bool IsAddressSection(ULONG_PTR address, ULONG_PTR Baseaddress, IMAGE_SECTION_HEADER* pSHeader, IMAGE_NT_HEADERS* ntheader, void** OutSBaseAddress,
		SIZE_T* OutSSize) 
	{
		for (int i = 0; i < ntheader->FileHeader.NumberOfSections; i++)
		{
			if ((pSHeader->VirtualAddress <= (address - Baseaddress)) && ((address - Baseaddress) < (pSHeader->VirtualAddress + pSHeader->Misc.VirtualSize))) {
				*OutSBaseAddress = (void*)((ULONG_PTR)Baseaddress + pSHeader->VirtualAddress);
				*OutSSize = pSHeader->Misc.VirtualSize;
				return true;
			}
			pSHeader++;
		}
		return false;
	}
	static size_t ConvertRvaToOffset(ULONG_PTR address, ULONG_PTR Baseaddress, IMAGE_SECTION_HEADER* pSHeader, IMAGE_NT_HEADERS* ntheader)
	{
		for (int i = 0; i < ntheader->FileHeader.NumberOfSections; i++)
		{
			if ((pSHeader->VirtualAddress <= (address - Baseaddress)) && ((address - Baseaddress) < (pSHeader->VirtualAddress + pSHeader->Misc.VirtualSize))) {
				return (address - Baseaddress - pSHeader->VirtualAddress) + (pSHeader->PointerToRawData);
			}
			pSHeader++;
		}
		return NULL;
	}
	static bool IsAddressPresent(void** Buffer, void* Address, size_t size)
	{
		void** Start = Buffer;
		for (size_t i = 0; i < size; i++, Start++) {
			if (*Start == Address) {
				return true;
			}
		}
		return false;
	}
	static CHAR SystemDirectory[MAX_PATH] = { 0 };
	static void HideAntiAntiAttach()
	{
		if (Hooks_Config::Anti_Anti_Attach)
		{
			// Vars
			void* Page = NULL;
			void** ExportsPage = NULL;
			void** ExportsPageDbg = NULL;

			size_t ExportsFunctions = NULL;
			size_t fileSize = NULL;

			std::FILE* FileP = NULL;

			// Check if the variable is empty 
			if (SystemDirectory[0] == NULL) {
				GetSystemDirectoryA(SystemDirectory, MAX_PATH);
				lstrcatA(SystemDirectory, "\\ntdll.dll");
			}

			// Read the file
			FileP = std::fopen(SystemDirectory, "rb");
			if (FileP)
			{
				// Get file size
				std::fseek(FileP, 0, SEEK_END);
				fileSize = std::ftell(FileP);
				std::fseek(FileP, 0, SEEK_SET);

				Page = std::malloc(fileSize);
				if (Page)
				{
					// Read the file and parse headers later
					std::fread(Page, fileSize, 1, FileP);

					// Headers
					auto pDosHeader = (IMAGE_DOS_HEADER*)Hooks_Informastion::hNtDll;
					if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
						std::free(Page);
						std::fclose(FileP);
						return;
					}
					auto pNtHeader = (IMAGE_NT_HEADERS*)((ULONG_PTR)Hooks_Informastion::hNtDll + pDosHeader->e_lfanew);
					if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
						std::free(Page);
						std::fclose(FileP);
						return;
					}
					if (pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size <= 0) {
						std::free(Page);
						std::fclose(FileP);
						return;
					}
					auto pExports = (IMAGE_EXPORT_DIRECTORY*)((ULONG_PTR)Hooks_Informastion::hNtDll + pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

					size_t fSize = sizeof(void*) * pExports->NumberOfNames;
					ExportsPage = (void**)std::malloc(fSize);
					void** exppage = ExportsPage;
					if (ExportsPage)
					{
						ExportsPageDbg = (void**)std::malloc(fSize);
						void** exppagedbg = ExportsPageDbg;
						if (ExportsPageDbg)
						{
							DWORD* NamesAddresses = (DWORD*)((ULONG_PTR)Hooks_Informastion::hNtDll + pExports->AddressOfNames);
							for (unsigned int i = 0; i < pExports->NumberOfNames; i++, NamesAddresses++) {
								LPCSTR StrP = (LPCSTR)((ULONG_PTR)Hooks_Informastion::hNtDll + *NamesAddresses);
								void* ExportsPageT = (void*)GetProcAddress(Hooks_Informastion::hNtDll, StrP);
								*exppage = ExportsPageT;
								if (std::memcmp(StrP, "Dbg", std::strlen("Dbg")) == 0) {
									*exppagedbg = ExportsPageT;
									exppagedbg++;
								}
								exppage++;
							}

							// Restore here
							size_t b = 0;
							void** exp = ExportsPageDbg;
							while (b < fSize && *exp)
							{
								size_t SectionVirtualSize = 0;
								void* OutSBaseAddress = nullptr;
								if (IsAddressSection((ULONG_PTR)*exp, (ULONG_PTR)Hooks_Informastion::hNtDll, IMAGE_FIRST_SECTION(pNtHeader), pNtHeader, &OutSBaseAddress, &SectionVirtualSize))
								{
									size_t Position = ((ULONG_PTR)*exp - (ULONG_PTR)OutSBaseAddress);
									size_t i = 0;
									for (;;)
									{
										ULONG_PTR CurrentAddress = (ULONG_PTR)*exp + i;

										if (Position > SectionVirtualSize) {
											break;
										}

										if (i != 0) {
											if (IsAddressPresent(ExportsPage, (void*)CurrentAddress, fSize)) {
												break;
											}
										}

										size_t offsetRaw = ConvertRvaToOffset(CurrentAddress, (ULONG_PTR)Hooks_Informastion::hNtDll, IMAGE_FIRST_SECTION(pNtHeader), pNtHeader);
										if (offsetRaw)
										{
											DWORD OLD;
											if (VirtualProtect((void*)CurrentAddress, sizeof(BYTE), PAGE_EXECUTE_READWRITE, &OLD))
											{
												std::memcpy((void*)CurrentAddress, (void*)((ULONG_PTR)Page + offsetRaw), sizeof(BYTE));
												VirtualProtect((void*)CurrentAddress, sizeof(BYTE), OLD, &OLD);
											}
										}
										i++;
										Position++;
									}
								}
								b++;
								exp++;
							}

							// free everything 
							std::free(Page);
							std::free(ExportsPage);
							std::free(ExportsPageDbg);
						}
						else {
							std::free(Page);
							std::free(ExportsPage);
						}
					}
					else {
						std::free(Page);
					}
				}
				std::fclose(FileP);
			}
		}
	}

	static void HideProcess32First()
	{
		Hooks_Informastion::Kernel32_Process32FirstWP = GetProcAddress(Hooks_Informastion::hKernel32, "Process32FirstW");
		
		if (Hooks_Config::Kernel32_Process32First)
		{
			if (Hooks_Informastion::Kernel32_Process32FirstWP)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Kernel32_Process32FirstWID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Kernel32_Process32FirstWP, Hook_emu::__Process32FirstW, &ErrorCode);

				if (Hooks_Informastion::Kernel32_Process32FirstWID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Kernel32_Process32FirstWID, &ErrorCode);
					Hooks_Informastion::Kernel32_Process32FirstWP = HookDataS.OriginalF;
				}
			}
		}
	}
	static void HideProcess32Next()
	{
		Hooks_Informastion::Kernel32_Process32NextWP = GetProcAddress(Hooks_Informastion::hKernel32, "Process32NextW");

		if (Hooks_Config::Kernel32_Process32Next)
		{
			if (Hooks_Informastion::Kernel32_Process32NextWP)
			{
				// Hook
				Hook_Info HookDataS;
				int32_t ErrorCode = NULL;

				Hooks_Informastion::Kernel32_Process32NextWID = ColdHook_Service::InitFunctionHookByAddress(&HookDataS, true,
					Hooks_Informastion::Kernel32_Process32NextWP, Hook_emu::__Process32NextW, &ErrorCode);

				if (Hooks_Informastion::Kernel32_Process32NextWID > NULL && ErrorCode == NULL)
				{
					ColdHook_Service::ServiceRegisterHookInformation(&HookDataS, Hooks_Informastion::Kernel32_Process32NextWID, &ErrorCode);
					Hooks_Informastion::Kernel32_Process32NextWP = HookDataS.OriginalF;
				}
			}
		}
	}
}
namespace Hooks_Manager
{
	// Init and ShutDown
	static bool IsInitted = false;
	void Init(ULONG_PTR hThisModule)
	{
		if (!IsInitted)
		{
			// ReadIniConfiguration first
			Hooks_Manager::ReadIni();

			// Initialiizzation
			if (ColdHook_Service::ServiceGlobalInit(NULL))
			{
				Hooks_Informastion::hNtDll = GetModuleHandleA("ntdll.dll");
				Hooks_Informastion::hKernel = GetModuleHandleA("kernelbase.dll");
				if (Hooks_Informastion::hKernel == NULL) {
					Hooks_Informastion::hKernel = GetModuleHandleA("kernel32.dll");
				}
				Hooks_Informastion::hKernel32 = GetModuleHandleA("kernel32.dll");
				
				// Store the current PID
				Hooks_Informastion::CurrentProcessID = GetCurrentProcessId();
				Hooks_Manager::GetExplorerPID();

				// Call hooking functions
				Hooks::HideProcessInformations();
				Hooks::HideCloseHandle();
				Hooks::HideDRx();
				Hooks::HidePEB();
				Hooks::HideQueryObject();
				Hooks::HideQuerySystemInformation();
				Hooks::HideSetInformationThread();
				Hooks::HideSetInformationProcess();
				Hooks::HideNtContinue();
				Hooks::HideCreateThreadEx();
				Hooks::HideExceptionDispatcher();
				Hooks::HideYieldExecution();
				Hooks::HideSetDebugFilterState();

				Hooks::HideProcess32First();
				Hooks::HideProcess32Next();

				Hooks::HideAntiAntiAttach();

				Hook_emu::InitHookFunctionsVars();

				IsInitted = true;
			}
		}
	}
	void ShutDown()
	{
		return;
	}

	// Configuration
	static void ReadIni()
	{
		char INI[MAX_PATH] = { 0 };
		std::strcpy(INI, ColdHisdepath);
		std::strcat(INI, "ColdHide.ini");

		Hooks_Config::HideWholePEB = GetPrivateProfileIntA("PEB_Hook", "HideWholePEB", true, INI) != FALSE;

		if (!Hooks_Config::HideWholePEB)
		{
			Hooks_Config::PEB_BeingDebugged = GetPrivateProfileIntA("PEB_Hook", "BeingDebugged", true, INI) != FALSE;
			Hooks_Config::PEB_NtGlobalFlag = GetPrivateProfileIntA("PEB_Hook", "NtGlobalFlag", true, INI) != FALSE;
			Hooks_Config::HeapFlags = GetPrivateProfileIntA("PEB_Hook", "HeapFlags", true, INI) != FALSE;
		}
		else
		{
			Hooks_Config::HideWholePEB = false;
			Hooks_Config::PEB_BeingDebugged = true;
			Hooks_Config::PEB_NtGlobalFlag = true;
			Hooks_Config::HeapFlags = true;
		}
		Hooks_Config::HideWholeDRx = GetPrivateProfileIntA("Nt_DRx", "HideWholeDRx", true, INI) != FALSE;
		Hooks_Config::FakeContextEmulation = GetPrivateProfileIntA("Nt_DRx", "FakeContextEmulation", true, INI) != FALSE;
		if (!Hooks_Config::HideWholeDRx)
		{
			Hooks_Config::DRx_ThreadContextRead = GetPrivateProfileIntA("Nt_DRx", "NtGetContextThread", true, INI) != FALSE;
			Hooks_Config::DRx_ThreadContextWrite = GetPrivateProfileIntA("Nt_DRx", "NtSetContextThread", true, INI) != FALSE;
			Hooks_Config::Nt_Continue = GetPrivateProfileIntA("Nt_DRx", "NtContinue", true, INI) != FALSE;
			Hooks_Config::Nt_KiUserExceptionDispatcher = GetPrivateProfileIntA("Nt_DRx", "KiUserExceptionDispatcher", true, INI) != FALSE;
		}
		else
		{
			Hooks_Config::HideWholeDRx = false;
			Hooks_Config::DRx_ThreadContextRead = true;
			Hooks_Config::DRx_ThreadContextWrite = true;
			Hooks_Config::Nt_Continue = true;
			Hooks_Config::Nt_KiUserExceptionDispatcher = true;
		}
		Hooks_Config::Anti_Anti_Attach = GetPrivateProfileIntA("Additional", "Anti_Anti_Attach", false, INI) != FALSE;

		Hooks_Config::Nt_QueryProcess = GetPrivateProfileIntA("NTAPIs", "NtQueryInformationProcess", true, INI) != FALSE;
		Hooks_Config::Nt_QuerySystem = GetPrivateProfileIntA("NTAPIs", "NtQuerySystemInformation", true, INI) != FALSE;
		Hooks_Config::Nt_SetThreadInformation = GetPrivateProfileIntA("NTAPIs", "NtSetInformationThread", true, INI) != FALSE;
		Hooks_Config::Nt_Close = GetPrivateProfileIntA("NTAPIs", "NtClose", true, INI) != FALSE;
		Hooks_Config::Nt_QueryObject = GetPrivateProfileIntA("NTAPIs", "NtQueryObject", true, INI) != FALSE;
		Hooks_Config::Nt_CreateThreadEx = GetPrivateProfileIntA("NTAPIs", "NtCreateThreadEx", true, INI) != FALSE;
		Hooks_Config::Nt_SetInformationProcess = GetPrivateProfileIntA("NTAPIs", "NtSetInformationProcess", true, INI) != FALSE;
		Hooks_Config::Nt_YieldExecution = GetPrivateProfileIntA("NTAPIs", "NtYieldExecution", true, INI) != FALSE;
		Hooks_Config::Nt_SetDebugFilterState = GetPrivateProfileIntA("NTAPIs", "NtSetDebugFilterState", true, INI) != FALSE;

		Hooks_Config::Kernel32_Process32First = GetPrivateProfileIntA("WinAPIs", "Process32First", true, INI) != FALSE;
		Hooks_Config::Kernel32_Process32Next = GetPrivateProfileIntA("WinAPIs", "Process32Next", true, INI) != FALSE;
	}
	static std::multimap<DWORD, size_t> ThreadDataID;
	static size_t CurrentThreadDataIDP = 0;

	void GetExplorerPID()
	{
		// Get fake PID.
		HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (hProcessSnap != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32 pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32);

			if (Process32First(hProcessSnap, &pe32))
			{
				do
				{
					if (lstrcmp(pe32.szExeFile, L"explorer.exe") == 0)
					{
						Hooks_Informastion::FPPID = pe32.th32ProcessID;
						break;
					}
				} while (Process32Next(hProcessSnap, &pe32) == TRUE);
				CloseHandle(hProcessSnap);
			}
		}
	}
	size_t GetOffsetByThreadID(DWORD ID)
	{
		if (ThreadDataID.empty()) 
		{
			ThreadDataID.insert(std::make_pair(ID, CurrentThreadDataIDP));
			return CurrentThreadDataIDP;
		}
		else 
		{
			for (auto iter = ThreadDataID.begin(); iter != ThreadDataID.end(); iter++)
			{
				if (iter->first == ID) {
					return iter->second;
				}
			}
			CurrentThreadDataIDP++;
			ThreadDataID.insert(std::make_pair(ID, CurrentThreadDataIDP));
			return CurrentThreadDataIDP;
		}
	}
}