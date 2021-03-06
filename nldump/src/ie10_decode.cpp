
 
#define UNICODE
#include <windows.h>

#include <cstdio>
#include <string>

#pragma comment(lib, "user32.lib")

VOID showError(DWORD dwError, PWCHAR pFmt, ...) {
  PWCHAR pDetails;
  WCHAR buffer[2048];
  
  if (pFmt != NULL) {
    va_list arglist;
    va_start(arglist, pFmt);
		wvsprintf(buffer, pFmt, arglist);
		va_end(arglist);
  }
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
      (LPWSTR)&pDetails, 0, NULL);

  wprintf(L"\n  %s : %s", buffer, pDetails);
  LocalFree(pDetails);
}

typedef HANDLE HVAULT;

#define VAULT_ENUMERATE_ALL_ITEMS 512

GUID Vault_WebCredential_ID = 
{ 0x3CCD5499, 0x87A8, 0x4B10, 0xA2, 0x15, 0x60, 0x88, 0x88, 0xDD, 0x3B, 0x55 };

enum VAULT_ELEMENT_TYPE {
  ElementType_Boolean = 0,
  ElementType_Short = 1,
  ElementType_UnsignedShort = 2,
  ElementType_Integer = 3,
  ElementType_UnsignedInteger = 4,
  ElementType_Double = 5,
  ElementType_Guid = 6,
  ElementType_String = 7,
  ElementType_ByteArray = 8,
  ElementType_TimeStamp = 9,
  ElementType_ProtectedArray = 0xA,
  ElementType_Attribute = 0xB,
  ElementType_Sid = 0xC,
  ElementType_Last = 0xD,
  ElementType_Undefined = 0xFFFFFFFF
};

enum VAULT_SCHEMA_ELEMENT_ID {
  ElementId_Illegal = 0,
  ElementId_Resource = 1,
  ElementId_Identity = 2,
  ElementId_Authenticator = 3,
  ElementId_Tag = 4,
  ElementId_PackageSid = 5,
  ElementId_AppStart = 0x64,
  ElementId_AppEnd = 0x2710
};

typedef struct _VAULT_CAUB {
  DWORD NumBytes;
  PBYTE pByteArray;
} VAULT_CAUB, *PVAULT_CAUB;

typedef struct _VAULT_VARIANT {
  VAULT_ELEMENT_TYPE Type;
  DWORD Unknown;
  union {
    BOOL Boolean;
    WORD Short;
    WORD UnsignedShort;
    DWORD Int;
    DWORD UnsignedInt;
    double Double;
    GUID Guid;
    LPCWSTR String;
    VAULT_CAUB ByteArray;
    VAULT_CAUB ProtectedArray;
    DWORD Attribute;
    DWORD Sid;
  } vv;
} VAULT_VARIANT, *PVAULT_VARIANT;

typedef struct _VAULT_ITEM_ELEMENT {
  VAULT_SCHEMA_ELEMENT_ID SchemaElementId;
  DWORD Unknown;
  VAULT_VARIANT ItemValue;
} VAULT_ITEM_ELEMENT, *PVAULT_ITEM_ELEMENT;

typedef struct _VAULT_ITEM_W7 {
  GUID SchemaId;
  LPCWSTR pszCredentialFriendlyName;
  PVAULT_ITEM_ELEMENT pResourceElement;
  PVAULT_ITEM_ELEMENT pIdentityElement;
  PVAULT_ITEM_ELEMENT pAuthenticatorElement;
  FILETIME LastModified;
  DWORD dwFlags;
  DWORD dwPropertiesCount;
  PVAULT_ITEM_ELEMENT pPropertyElements;
} VAULT_ITEM_W7, *PVAULT_ITEM_W7;

typedef struct _VAULT_ITEM_W8 {
  GUID SchemaId;
  LPCWSTR pszCredentialFriendlyName;
  PVAULT_ITEM_ELEMENT pResourceElement;
  PVAULT_ITEM_ELEMENT pIdentityElement;
  PVAULT_ITEM_ELEMENT pAuthenticatorElement;
  PVAULT_ITEM_ELEMENT pPackageSid;
  FILETIME LastModified;
  DWORD dwFlags;
  DWORD dwPropertiesCount;
  PVAULT_ITEM_ELEMENT pPropertyElements;
} VAULT_ITEM_W8, *PVAULT_ITEM_W8;

typedef struct _VAULT_ITEM {
  std::wstring Account;
  std::wstring Url;
  FILETIME LastModified;
  std::wstring UserName;
  std::wstring Password;
} VAULT_ITEM, *PVAULT_ITEM;

typedef DWORD (WINAPI *VaultEnumerateVaults)(DWORD dwFlags, PDWORD VaultsCount, GUID **ppVaultGuids);
typedef DWORD (WINAPI *VaultEnumerateItems)(HVAULT VaultHandle, DWORD dwFlags, PDWORD ItemsCount, PVOID *Items);
typedef DWORD (WINAPI *VaultOpenVault)(GUID *pVaultId, DWORD dwFlags, HVAULT *pVaultHandle);
typedef DWORD (WINAPI *VaultCloseVault)(HVAULT VaultHandle);
typedef DWORD (WINAPI *VaultFree)(PVOID pMemory);
typedef DWORD (WINAPI *VaultGetItemW7)(HVAULT VaultHandle, GUID *pSchemaId, PVAULT_ITEM_ELEMENT pResource, PVAULT_ITEM_ELEMENT pIdentity, HWND hwndOwner, DWORD dwFlags, PVAULT_ITEM_W7 *ppItem);
typedef DWORD (WINAPI *VaultGetItemW8)(HVAULT VaultHandle, GUID *pSchemaId, PVAULT_ITEM_ELEMENT pResource, PVAULT_ITEM_ELEMENT pIdentity, PVAULT_ITEM_ELEMENT pPackageSid, HWND hwndOwner, DWORD dwFlags, PVAULT_ITEM_W8 *ppItem);

HMODULE hVaultLib;

VaultEnumerateItems  pVaultEnumerateItems;
VaultFree            pVaultFree;
VaultGetItemW7       pVaultGetItemW7;
VaultGetItemW8       pVaultGetItemW8;
VaultOpenVault       pVaultOpenVault;
VaultCloseVault      pVaultCloseVault;
VaultEnumerateVaults pVaultEnumerateVaults;

BOOL InitVault(VOID) {
	
	BOOL bStatus = FALSE;
	
	hVaultLib = LoadLibrary(L"vaultcli.dll");
	
	if (hVaultLib != NULL) {
    pVaultEnumerateItems  = (VaultEnumerateItems)GetProcAddress(hVaultLib,  "VaultEnumerateItems");
		pVaultEnumerateVaults = (VaultEnumerateVaults)GetProcAddress(hVaultLib, "VaultEnumerateVaults");
		pVaultFree            = (VaultFree)GetProcAddress(hVaultLib,            "VaultFree");
		pVaultGetItemW7       = (VaultGetItemW7)GetProcAddress(hVaultLib,       "VaultGetItem");
		pVaultGetItemW8       = (VaultGetItemW8)GetProcAddress(hVaultLib,       "VaultGetItem");
		pVaultOpenVault       = (VaultOpenVault)GetProcAddress(hVaultLib,       "VaultOpenVault");
		pVaultCloseVault      = (VaultCloseVault)GetProcAddress(hVaultLib,      "VaultCloseVault");
		
		bStatus = (pVaultEnumerateVaults != NULL)
			   && (pVaultFree != NULL)
         && (pVaultGetItemW7 != NULL)
         && (pVaultGetItemW8 != NULL)
			   && (pVaultOpenVault != NULL)
			   && (pVaultCloseVault != NULL)
         && (pVaultEnumerateItems != NULL);
	}
	return bStatus;
}

BOOL IsOs_Win80rGreater(VOID) {
  OSVERSIONINFO info;

  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);

  return (info.dwPlatformId == 2 && info.dwMajorVersion > 6)
     || (info.dwMinorVersion > 1 && info.dwMajorVersion == 6);
}

BOOL GetItemW7(HVAULT hVault, PVAULT_ITEM_W7 ppItems, DWORD index, VAULT_ITEM &item) {
  DWORD dwError;
  
  // is this a web credential?
  if (memcmp(&Vault_WebCredential_ID, &ppItems[index].SchemaId, sizeof(GUID)) == 0) {
    item.Account = ppItems[index].pszCredentialFriendlyName;
    item.Url = ppItems[index].pResourceElement->ItemValue.vv.String;
    item.UserName = ppItems[index].pIdentityElement->ItemValue.vv.String;
    memcpy(&item.LastModified, &ppItems[index].LastModified, sizeof(FILETIME));
    
    if (ppItems[index].dwPropertiesCount == 0) {
      PVAULT_ITEM_W7 ppCredentials = NULL;
      dwError = pVaultGetItemW7(hVault, 
            &ppItems[index].SchemaId, ppItems[index].pResourceElement,
            ppItems[index].pIdentityElement, NULL, 0, &ppCredentials);
      if (dwError == ERROR_SUCCESS) {
        item.Password = ppCredentials->pAuthenticatorElement->ItemValue.vv.String;
        pVaultFree(ppCredentials);
      }
    }
  }
  return dwError == ERROR_SUCCESS;
}

BOOL GetItemW8(HVAULT hVault, PVAULT_ITEM_W8 ppItems, DWORD index, VAULT_ITEM &item) {
  DWORD dwError;
  
  // is this a web credential?
  if (memcmp(&Vault_WebCredential_ID, &ppItems[index].SchemaId, sizeof(GUID)) == 0) {
    item.Account = ppItems[index].pszCredentialFriendlyName;
    item.Url = ppItems[index].pResourceElement->ItemValue.vv.String;
    item.UserName = ppItems[index].pIdentityElement->ItemValue.vv.String;
    memcpy(&item.LastModified, &ppItems[index].LastModified, sizeof(FILETIME));
    
    if (ppItems[index].dwPropertiesCount == 0) {
      PVAULT_ITEM_W8 ppCredentials = NULL;
      dwError = pVaultGetItemW8(hVault, 
            &ppItems[index].SchemaId, ppItems[index].pResourceElement,
            ppItems[index].pIdentityElement, NULL, NULL, 0, &ppCredentials);
      if (dwError == ERROR_SUCCESS) {
        item.Password = ppCredentials->pAuthenticatorElement->ItemValue.vv.String;
        pVaultFree(ppCredentials);
      }
    }
  }
  return dwError == ERROR_SUCCESS;
}

void ListWebCredentials(void) {
	DWORD dwVaults, dwError;
  HVAULT hVault;
  GUID *ppVaultGuids;
  BOOL bWin80rGreater = IsOs_Win80rGreater();
  
  dwError = pVaultEnumerateVaults(NULL, &dwVaults, &ppVaultGuids);

  if (dwError != ERROR_SUCCESS) {
    showError(dwError, L"VaultEnumerateVaults");
    return;
  }
  
  // for each vault found
  for (DWORD i = 0;i < dwVaults;i++) {
    dwError = pVaultOpenVault(&ppVaultGuids[i], 0, &hVault);
    // open it
    if (dwError == ERROR_SUCCESS) {
      PVOID ppItems;
      DWORD dwItems;
      
      // enumerate items
      dwError = pVaultEnumerateItems(hVault, VAULT_ENUMERATE_ALL_ITEMS, 
          &dwItems, &ppItems);
      
      if (dwError == ERROR_SUCCESS) {
        // for each item
        for (DWORD j = 0; j < dwItems; j++) {
          VAULT_ITEM item;
          BOOL bResult = FALSE;
          memset(&item, 0, sizeof(VAULT_ITEM));
          
          if (bWin80rGreater) {
            bResult = GetItemW8(hVault, (PVAULT_ITEM_W8)ppItems, j, item);
          } else {
            bResult = GetItemW7(hVault, (PVAULT_ITEM_W7)ppItems, j, item);
          }
          
          if (bResult) {
            // application
            wprintf(L"\n  Account name: %s", item.Account.c_str());
                
            // web address
            wprintf(L"\n  Website address (URL): %s", item.Url.c_str());
              
            FILETIME ft, lt;
            SYSTEMTIME st;
            wchar_t modified[MAX_PATH];
            
            FileTimeToLocalFileTime(&item.LastModified, &lt);
            FileTimeToSystemTime(&lt, &st);

            // last time it was updated
            if (GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0,
                &st, L"MM/dd/yyyy", modified, MAX_PATH)) {
              wprintf(L"\n  Last modified: %s", modified);
            }
            wprintf(L"\n  User Name: %s", item.UserName.c_str());
            wprintf(L"\n  Password: %s\n", item.Password.c_str());
          }
        }
        pVaultFree(ppItems);
      } else {
        showError(dwError, L"VaultEnumerateItems()");
      }
      pVaultCloseVault(hVault);
    } else {
      showError(dwError, L"VaultOpenVault()");
    }
  }
}

VOID ConsoleSetBufferWidth(SHORT X) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  
  if (X <= csbi.dwSize.X) return;
  csbi.dwSize.X = X;
  SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csbi.dwSize);  
}

int wmain(void) {
  ConsoleSetBufferWidth(300);
 
	if (InitVault()) {
    ListWebCredentials();
  } else {
    wprintf(L"\n  Failed to initialize Vault API\n");
  }
  wprintf(L"\n\n  Press any key to continue . . .");
  fgetc(stdin);
	return 0;
}
