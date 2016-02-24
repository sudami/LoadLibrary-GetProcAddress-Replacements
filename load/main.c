#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winternl.h>
#include <intrin.h> 

HMODULE WINAPI GetModuleBaseAddress(LPCWSTR moduleName)
{
#ifdef _M_IX86 
	PPEB pPeb = (PPEB)__readfsdword(0x30);
#elif _M_AMD64
	PPEB pPeb = (PPEB)__readgsqword(0x60);
#endif

	PLDR_DATA_TABLE_ENTRY pLdrDataTableEntry = (PLDR_DATA_TABLE_ENTRY)pPeb->Ldr->InMemoryOrderModuleList.Flink;

	PLIST_ENTRY pListEntry, pListFirst;
	pListEntry = pListFirst = pPeb->Ldr->InMemoryOrderModuleList.Flink; 

	do
	{
		if (lstrcmpiW(pLdrDataTableEntry->FullDllName.Buffer, moduleName) == 0)
			return (HMODULE)pLdrDataTableEntry->Reserved2[0];

		pListEntry = pListEntry->Flink;
		pLdrDataTableEntry = (PLDR_DATA_TABLE_ENTRY)(pListEntry->Flink);

	} while (pListEntry != pListFirst);

	return NULL;
} 

FARPROC WINAPI GetExportAddress(HMODULE hMod, const char *lpProcName)
{
	char *pBaseAddress = (char *)hMod;

	IMAGE_DOS_HEADER *pDosHeader = (IMAGE_DOS_HEADER *)pBaseAddress;
	IMAGE_NT_HEADERS *pNtHeaders = (IMAGE_NT_HEADERS *)(pBaseAddress + pDosHeader->e_lfanew);
	IMAGE_OPTIONAL_HEADER *pOptionalHeader = &pNtHeaders->OptionalHeader;
	IMAGE_DATA_DIRECTORY *pDataDirectory = (IMAGE_DATA_DIRECTORY *)(&pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]);
	IMAGE_EXPORT_DIRECTORY *pExportDirectory = (IMAGE_EXPORT_DIRECTORY *)(pBaseAddress + pDataDirectory->VirtualAddress);

	void **ppFunctions = (void **)(pBaseAddress + pExportDirectory->AddressOfFunctions);
	WORD *pOrdinals = (WORD *)(pBaseAddress + pExportDirectory->AddressOfNameOrdinals);
	//char **pNames = (char **)(pBaseAddress + pExportDirectory->AddressOfNames);
	ULONG *pNames = (ULONG *)(pBaseAddress + pExportDirectory->AddressOfNames);

	void *pAddress = NULL;
	LoadLibraryAF pLoadLibraryA = NULL;

	DWORD i;

	if (((DWORD_PTR)lpProcName >> 16) == 0) 
	{
		WORD ordinal = LOWORD(lpProcName);
		DWORD dwOrdinalBase = pExportDirectory->Base; 

		if (ordinal < dwOrdinalBase || ordinal >= dwOrdinalBase + pExportDirectory->NumberOfFunctions)
			return NULL;
		 
		pAddress = (FARPROC)(pBaseAddress + (DWORD_PTR)ppFunctions[ordinal - dwOrdinalBase]);
	}
	else 
	{ 
		for (i = 0; i < pExportDirectory->NumberOfNames; i++) 
		{ 
			char *szName = (char*)pBaseAddress + (DWORD_PTR)pNames[i];
			if (strcmp(lpProcName, szName) == 0)
			{
				pAddress = (FARPROC)(pBaseAddress + ((ULONG*)(pBaseAddress + pExportDirectory->AddressOfFunctions))[pOrdinals[i]]);
				break;
			}
		}
	}
	
	if ((char *)pAddress >= (char *)pExportDirectory && (char *)pAddress < (char *)pExportDirectory + pDataDirectory->Size) 
	{
		char *szDllName, *szFunctionName;
		HMODULE hForward;
		
		szDllName = _strdup((const char *)pAddress);
		if (!szDllName)
			return NULL;

		pAddress = NULL;
		szFunctionName = strchr(szDllName, '.');
		*szFunctionName++ = 0; 

		pLoadLibraryA = (LoadLibraryAF)GetExportAddress(GetModuleBaseAddress(L"KERNEL32.DLL"), "LoadLibraryA");

		if (pLoadLibraryA == NULL)
			return NULL;

		hForward = pLoadLibraryA(szDllName);

		if (hForward)
			pAddress = GetExportAddress(hForward, szFunctionName);

		free(szDllName);
	}
	return pAddress;
}

int main()
{
	HMODULE hKernel32 = GetModuleBaseAddress(L"KERNEL32.DLL"); 
	LoadLibraryAF pLoadLibraryA = (LoadLibraryAF)GetExportAddress(hKernel32, "LoadLibraryA");
	GetProcAddressF pGetProcAddress = (GetProcAddressF)GetExportAddress(hKernel32, "GetProcAddress");
	typedef HMODULE(WINAPI *LoadLibraryAF)(LPCSTR lpFileName);
	typedef FARPROC(WINAPI *GetProcAddressF)(HMODULE hModule, LPCSTR lpProcName);
	typedef HMODULE(WINAPI *GetModuleHandleWF)(LPCWSTR lpModuleName); 

	HMODULE hUser32 = pLoadLibraryA("user32.dll"); 
	FARPROC pMessageBox = pGetProcAddress(hUser32, "MessageBoxW");

	pMessageBox(NULL, L"It works!", L"Hello World!", MB_OK);
	
	return 0;
}
