
// Win HTTP library using asynchronous calls
// queries virus total for scan report.
// need to implement upload option..
// Odzhan
//
// cl vt.cpp md5.c sha1.c sha256.c jsmn.c wsetargv.obj
/*
  [ checking HELKERN.000
41 virus scanner results
  [ detected by McAfee+Artemis       : W32/SQLSlammer.worm
  [ detected by McAfee               : W32/SQLSlammer.worm
  [ detected by TheHacker            : W32/SQLSlammer
  [ detected by VirusBuster          : Win32.SQLExp.A
  [ detected by a-squared            : Worm.Sql.Slammer.dmp!IK
  [ detected by Avast                : Win32:SQLSlammer
  [ detected by BitDefender          : Worm.Sql.Slammer.Dump.A
  [ detected by Comodo               : Worm.Win32.SQLSlammer.~A
  [ detected by DrWeb                : Win32.SQL.Slammer.368
  [ detected by AntiVir              : Worm/Sql.Slammer.dmp
  [ detected by McAfee-GW-Edition    : Heuristic.BehavesLike.Exploit.CodeExec.FFLG
  [ detected by eTrust-Vet           : Win32/SQLSlammer
  [ detected by Antiy-AVL            : Worm/Win32.Slammer
  [ detected by Symantec             : W32.SQLExp.Worm.dump
  [ detected by Microsoft            : Worm:Win32/SQLSlammer.remnants
  [ detected by GData                : Worm.Sql.Slammer.Dump.A
  [ detected by PCTools              : Win32.SQLExp.A
  [ detected by Ikarus               : Worm.Sql.Slammer.dmp
  [ detected by Fortinet             : W32/SQLSlammer!worm
  [ detected by AVG                  : SQLSlammer
  [ detected by Panda                : Worm Generic
  
*/

#define UNICODE

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>

#include <sys/stat.h>

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include "md5.h"
#include "sha1.h"
#include "sha2.h"

#define MD5_HASH    0
#define SHA1_HASH   1
#define SHA256_HASH 2

typedef int hash_len_t;
typedef void *hash_ctx_t;
typedef void (*hash_init_t) (hash_ctx_t*);
typedef void (*hash_update_t) (hash_ctx_t*,void*,int);
typedef void (*hash_final_t) (void*,hash_ctx_t*);

typedef struct _hash_lib_t {
  char          *s;
  hash_len_t    hash_len;
  hash_ctx_t    *hash_ctx;
  hash_init_t   hash_init;
  hash_update_t hash_update;
  hash_final_t  hash_final;
} hash_lib_t;

MD5_CTX md5_ctx;
SHA1_CTX sha1_ctx;
SHA256_CTX sha256_ctx;

hash_lib_t hashlib[3] =
{ { "MD5",
    MD5_DIGEST_LENGTH,
    (hash_ctx_t*)&md5_ctx,
    (hash_init_t)MD5_Init, 
    (hash_update_t)MD5_Update, 
    (hash_final_t)MD5_Final 
  },
  { "SHA-1",
    SHA1_DIGEST_LENGTH,
    (hash_ctx_t*)&sha1_ctx,
    (hash_init_t)SHA1_Init, 
    (hash_update_t)SHA1_Update, 
    (hash_final_t)SHA1_Final 
  },
  { "SHA-256",
    SHA256_DIGEST_LENGTH,
    (hash_ctx_t*)&sha256_ctx,
    (hash_init_t)SHA256_Init, 
    (hash_update_t)SHA256_Update, 
    (hash_final_t)SHA256_Final
  } 
};

#pragma comment (lib, "winhttp.lib")
#pragma comment (lib, "shell32.lib")
#pragma comment (lib, "user32.lib")

#define HTTP_INTERNET 0
#define HTTP_CONNECT  1
#define HTTP_REQUEST  2

// you put your own API key here
wchar_t apikey[]=L"0a441ab5bbea7c277be4bd0774a364492944f2b978c3525afeb7e890b6da37ab";

wchar_t vt_path[] = L"/vtapi/v2/file/report?apikey=%s&resource=%s";
  
typedef struct http_code_t {
  DWORD dwCode;
  wchar_t *msg;
} http_code;

HINTERNET ih[8]={NULL};
HANDLE evt[8]={NULL};
DWORD icnt=0, ecnt=0;
DWORD total_len, data_len;
PBYTE buf=NULL;
DWORD evt_src=0, evt_rr=0, evt_rc=0, evt_da=0, evt_ha=0;
uint32_t start=0, current;
  
http_code http_status[] =
{ { WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION,      L"closing" },
  { WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER,     L"connected" },
  { WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER,    L"connecting" },
  { WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED,       L"connection closed" },
  { WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE,          L"data available" },
  { WINHTTP_CALLBACK_STATUS_HANDLE_CREATED,          L"handle created" },
  { WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING,          L"handle closing" },
  { WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,       L"headers available" },
  { WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE,   L"intermediate response" },
  { WINHTTP_CALLBACK_STATUS_NAME_RESOLVED,           L"name resolved" },
  { WINHTTP_CALLBACK_STATUS_READ_COMPLETE,           L"read complete" },
  { WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE,      L"receiving response" },
  { WINHTTP_CALLBACK_STATUS_REDIRECT,                L"redirect" },
  { WINHTTP_CALLBACK_STATUS_REQUEST_ERROR,           L"request error" },
  { WINHTTP_CALLBACK_STATUS_REQUEST_SENT,            L"request sent" },
  { WINHTTP_CALLBACK_STATUS_RESOLVING_NAME,          L"resolving name" },
  { WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED,       L"response received" },
  { WINHTTP_CALLBACK_STATUS_SECURE_FAILURE,          L"secure failure" },
  { WINHTTP_CALLBACK_STATUS_SENDING_REQUEST,         L"sending request" },
  { WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,    L"send request complete" },
  { WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE,          L"write complete" }
  //{ WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE, L"get proxy for url complete" },
  //{ WINHTTP_CALLBACK_STATUS_CLOSE_COMPLETE,          L"close complete" },
  //{ WINHTTP_CALLBACK_STATUS_SHUTDOWN_COMPLETE,       L"shutdown complete" }
};

http_code cert_errors[] =
{ { WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED,        L"unable to verify if revoked cert" },
  { WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT,           L"invalid cert" },
  { WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED,           L"verified revoked cert" },
  { WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA,             L"invalid cert authority" },
  { WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID,        L"invalid common name" },
  { WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID,      L"cert expired" },
  { WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR, L"internal error" }
};

wchar_t *code2str (DWORD dwCode, http_code codes[], DWORD len)
{
  DWORD i;
  for (i=0; i<len; i++) {
    if (dwCode==codes[i].dwCode) {
      return codes[i].msg;
    }
  }
  return L"unrecognized code";
}

// all status messages go here
void CALLBACK http_cb (HINTERNET hInternet, 
  DWORD_PTR dwContext, DWORD dwInternetStatus, 
  LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
  DWORD e = -1;
  
  //wprintf (L"\n  [ %s", code2str (dwInternetStatus, http_status, 
  //  sizeof (http_status) / sizeof (http_code)));
  
  if (dwInternetStatus==WINHTTP_CALLBACK_STATUS_HANDLE_CREATED)
  {
    //ih[icnt++] = hInternet;
  } else if (dwInternetStatus==WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE)
  {
    e=evt_src;
  } else if (dwInternetStatus==WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED)
  {
    e=evt_rr;
  } else if (dwInternetStatus==WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
  {
    data_len=dwStatusInformationLength;
    e=evt_rc;
  } else if (dwInternetStatus==WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)
  {
    data_len=*(PDWORD)lpvStatusInformation;
    e=evt_da;
  } else if (dwInternetStatus==WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)
  {
    e=evt_ha;
  }
  if (e != -1) {
    SetEvent (evt[e]);
  }
}

DWORD evt_open (BOOL bManualReset, BOOL bInitialState)
{
  evt[ecnt++] = CreateEvent (NULL, bManualReset, bInitialState, NULL);
  return ecnt-1;
}

void evt_close (void)
{
  ecnt--;
  CloseHandle (evt[ecnt]);
}

int http_open (LPCWSTR host)
{
  // open internet for async requests using default agent (NULL)
  ih[icnt++]=WinHttpOpen (NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
  
  if (ih[HTTP_INTERNET]!=NULL)
  {
    // install callback
    WinHttpSetStatusCallback (ih[HTTP_INTERNET], http_cb, 
      WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, NULL);
    
    // open connection
    ih[icnt++]=WinHttpConnect (ih[HTTP_INTERNET], host, 
      INTERNET_DEFAULT_HTTPS_PORT, 0);
  }
  return ih[HTTP_CONNECT] != NULL;
}

void req_close (void)
{
  if (ih[HTTP_REQUEST] != NULL) {
    WinHttpCloseHandle (ih[HTTP_REQUEST]);
    ih[HTTP_REQUEST]=NULL;
    icnt--;
  }
}

int req_open (wchar_t method[], wchar_t res[])
{
  req_close();
  
  ih[icnt++]=WinHttpOpenRequest (ih[HTTP_CONNECT], method, res, 
      NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 
      WINHTTP_FLAG_SECURE);
      
  if (ih[HTTP_REQUEST]!=NULL)
  {
    WinHttpSendRequest (ih[HTTP_REQUEST], 
      L"Content-Type: application/json; charset=utf-8", 0, NULL, 0, 0, 0);
  }
  return ih[HTTP_REQUEST] != NULL;
}

int check_hdrs (void)
{
  wchar_t hdrs[BUFSIZ];
  DWORD len=4;
  DWORD status=0;
  
  // return status code as integer
  WinHttpQueryHeaders (ih[HTTP_REQUEST], 
    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
    WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
  
  if (status!=HTTP_STATUS_OK)
  {
    printf ("  [ server response code %i\n", status);
  }
  return status==HTTP_STATUS_OK;
  
  /*
  if (WinHttpQueryHeaders (ih[HTTP_REQUEST], 
    WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
    hdrs, &len, WINHTTP_NO_HEADER_INDEX))
  {
    wprintf (L"\n%s", hdrs);
  }*/
}

#include "jsmn.h"

#define TOKEN_PRINT(t) \
	printf("start: %d, end: %d, type: %d, size: %d\n", \
			(t).start, (t).end, (t).type, (t).size)
      
void parse_json (void *js, size_t len)
{
  jsmn_parser jp;
  jsmnerr_t   r;
  int         i, j, k, vs=0, prop;
  jsmntok_t   tokens[1024];
  char res[256];
  const char *ds, *dv, *rv, *scanner;
  int field_len, value_len, scanner_len;
  int ds_len, dv_len, rv_len;
  
  memset (tokens, 0, sizeof (tokens));
  
  jsmn_init (&jp);
  r = jsmn_parse (&jp, (const char*)js, len, tokens, 1024);
  
  if (r<0)
  {
    switch (r)
    {
      case JSMN_ERROR_INVAL:
        printf ("  [ invalid response\n");
        break;
      case JSMN_ERROR_NOMEM:
        printf ("  [ no memory\n");
        break;
      case JSMN_ERROR_PART:
        printf ("  [ incomplete response\n");
        break;
    }
  } else {
    // should check if response code exists for files that haven't been processed by vt
    // display verbose message to user
    // {"response_code": 0, "resource": "<md5 hash>", "verbose_msg": 
    // "The requested resource is not among the finished, queued or pending scans"}
    
    if (r < 10) printf ("  [ %s\n", js);
    
    for (i = 1; tokens[i].end < tokens[0].end; i++) {
      if (tokens[i].type == JSMN_STRING || tokens[i].type == JSMN_PRIMITIVE) {
        if (strncmp ((const char*)js + tokens[i].start, "scans", strlen("scans"))==0) {
          vs=tokens[++i].size;
          printf ("%i virus scanner results\n", vs);
          // for each scanner
          for (j=0; j<vs; j++) {
            // get scanner name and len
            scanner=(const char*)js + tokens[i+1].start;
            scanner_len=tokens[i+1].end - tokens[i+1].start;
            //printf ("%.*s\n", tokens[i+1].end - tokens[i+1].start, (const char*)js + tokens[i+1].start);
            // get number of properties in this object
            prop=tokens[i+2].size;
            if (prop==4) {
              i+=3;
              // detection should be first
              ds=(const char*)js + tokens[i].start;
              ds_len=tokens[i].end - tokens[i].start;
              
              dv=(const char*)js + tokens[i+1].start;
              dv_len=tokens[i+1].end - tokens[i+1].start;
                
              // skip detection, version and result field
              i+=4;
              
              // now the result
              //rs=(const char*)js + tokens[i].start;
              //rs_len=tokens[i].end - tokens[i].start;
              
              rv=(const char*)js + tokens[i+1].start;
              rv_len=tokens[i+1].end - tokens[i+1].start;
              
              // if detected is true, print scanner name and result
              if (strncmp (ds, "detected", strlen ("detected"))==0) 
              {
                if (strncmp (dv, "true", strlen ("true"))==0) 
                {
                  if (scanner_len < sizeof (res)) {
                    strncpy (res, scanner, scanner_len);
                    res[scanner_len]=0;
                    printf ("  [ detected by %-20s : %.*s\n", 
                      res, rv_len, rv);
                  }
                  //break;
                }
              }
              i+=3;
              //printf ("%.*s = %.*s\n", field_len, field, value_len, value);*/
            } else {
              printf ("  [ invalid scanner object %i\n", prop);
              exit(-1);
            }
          }
          break;
        }
        //printf("%.*s\n", tokens[i].end - tokens[i].start, (const char*)js + tokens[i].start);
      } else if (tokens[i].type == JSMN_ARRAY) {
        printf("[%d array elems]\n", tokens[i].size);
      } else if (tokens[i].type == JSMN_OBJECT) {
        printf("{%i = %d object elems}\n", ++vs, tokens[i].size);
      } else {
        //TOKEN_PRINT(tokens[i]);
      }
    }
	}
}

void req_dispatch (void)
{
  DWORD  e;
  
  evt_src=evt_open (FALSE, FALSE);
  evt_rr=evt_open (FALSE, FALSE);
  evt_rc=evt_open (FALSE, FALSE);
  evt_da=evt_open (FALSE, FALSE);
  evt_ha=evt_open (FALSE, FALSE);
  
  total_len = data_len = 0;
  buf=NULL;
  
  do {
    e=WaitForMultipleObjects (ecnt, evt, FALSE, INFINITE);
  
    if (e == -1) break;
    
    // send request has completed, start to receive response
    if (e==evt_src)
    {
      WinHttpReceiveResponse (ih[HTTP_REQUEST], NULL);
    } 
    // headers are available, query volume of data available
    else if (e==evt_ha)
    {
      if (!check_hdrs()) {
        //printf ("  [ unexpected server response\n");
        break;
      }
      WinHttpQueryDataAvailable (ih[HTTP_REQUEST], NULL);
    }
    // data is available to read, reallocate new buffer
    else if (e==evt_da)
    {
      if (data_len==0) break;
      
      if (buf==NULL) {
        buf=(PBYTE)HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, data_len);
        if (buf==NULL) {
          printf (" [ error allocating memory\n");
          exit (-1);
        }
      } else {
        buf=(PBYTE)HeapReAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, buf, total_len + data_len);
        if (buf==NULL) {
          printf (" [ error allocating memory\n");
          exit (-1);
        }
      }
      WinHttpReadData (ih[HTTP_REQUEST], (LPVOID)&buf[total_len], data_len, NULL);
    } 
    // our read completed, update total length
    else if (e==evt_rc)
    {
      // if nothing read, exit loop
      if (data_len==0) break;
      
      total_len += data_len;
      // indicate we're ready to read more
      SetEvent (evt[evt_ha]);
    }
  } while (1);
  
  if (total_len!=0)
  { 
    parse_json (buf, total_len);
    HeapFree (GetProcessHeap(), 0, buf);
  }
  while (ecnt!=0) {
    evt_close();
  }
}
                                       
void http_close (void)
{
  // close request
  if (ih[HTTP_REQUEST]!=NULL) {
    WinHttpCloseHandle (ih[HTTP_REQUEST]);
    icnt--;
  }
  // close connection
  if (ih[HTTP_CONNECT]!=NULL) {
    WinHttpCloseHandle (ih[HTTP_CONNECT]);
    icnt--;
  }
  // close internet
  if (ih[HTTP_INTERNET]!=NULL) {
    WinHttpCloseHandle (ih[HTTP_INTERNET]);
    icnt--;
  }
}

void progress (uint64_t fs_complete, uint64_t fs_total)
{
  uint32_t total, hours=0, minutes=0, seconds=0, speed, avg;
  uint64_t pct;
  
  if (start==0) {
    start=time(0);
    return;
  }
  
  pct = (100 * fs_complete) / (1 * fs_total);
  
  total = (time(0) - start);
  
  if (total != 0) {
    // (remaining data * time elapsed) / data completed
    avg = (total * (fs_total - fs_complete)) / fs_complete;
    speed = (fs_complete / total);
    
    minutes = (avg / 60);
    seconds = (avg % 60);
  }
  printf ("\rProcessed %llu MB out of %llu MB %lu MB/s : %llu%% complete. ETA: %02d:%02d     ",
    fs_complete/1000/1000, fs_total/1000/1000, speed/1000/1000, pct, minutes, seconds);
}

// generate SHA-3 hash of file
int HASH_file (wchar_t fn[], wchar_t r[], int type)
{
  FILE     *fd;
  size_t   len, i;
  uint8_t  buf[BUFSIZ], dgst[256];
  struct _stat64i32 st;
  uint32_t cmp=0, total=0, dgstlen;
  start=current=0;

  // if exceeding 128MB, virus total probably
  // won't have signature anyway..
  _wstat (fn, &st);
  total=st.st_size;
  
  if (total >128*1024*1024) {
    printf ("  [ file exceeds 128MB\n");
    return 0;
  }
  
  fd = _wfopen (fn, L"rb");
  
  if (fd!=NULL)
  {  
    hashlib[type].hash_init (hashlib[type].hash_ctx);
    
    while (len = fread (buf, 1, BUFSIZ, fd)) {
      cmp += len;
      if ((cmp > 10000000) && (cmp % 10000000)==0 || (cmp==total)) {
        progress (cmp, total);
      }
      hashlib[type].hash_update (hashlib[type].hash_ctx, buf, len);
    }
    hashlib[type].hash_final (dgst, hashlib[type].hash_ctx);

    fclose (fd);
  } else {
    printf ("  [ unable to open %s\n", fn);
    return 0;
  }
  for (i=0; i<hashlib[type].hash_len; i++) {
    swprintf (&r[i*2], L"%02x", dgst[i]);
  }
  r[hashlib[type].hash_len*2]=0;
  return 1;
}

int wmain (int argc, wchar_t *argv[])
{
  //int argc;
  //wchar_t **argv;
  wchar_t params[BUFSIZ];
  wchar_t res[128];
  int i, hash_type=SHA256_HASH;
  
  //argv=CommandLineToArgvW(GetCommandLine(), &argc);
  
  if (argc<2) {
    wprintf (L"\n usage: vt_scan <file> /md5 /sha1 /sha2 (default)\n");
    return 0;
  }
  
  // check if parameters passed
  for (i=1; i<argc; i++)
  {
    if (argv[i][0]=='/')
    {
      if (wcsncmp (&argv[i][1], L"md5", 3)==0) {
        hash_type=MD5_HASH;
      } else if (wcsncmp (&argv[i][1], L"sha1", 4)==0) {
        hash_type=SHA1_HASH;
      } else if (wcsncmp (&argv[i][1], L"sha2", 4)==0) {
        hash_type=SHA256_HASH;
      } else {
        printf ("  [ unrecognized parameter : %s\n", argv[i]);
      }
    }
  }
  
  printf ("  [ using %s for hashing\n", hashlib[hash_type].s);
  
  // process all except paramters
  for (i=1; i<argc; i++) {
    if (argv[i][0] != '/') {
      printf ("  [ checking %ws\n", argv[i]);
      if (HASH_file (argv[i], res, hash_type)) {
        //printf ("  [ querying virus total\n");
        if (http_open (L"www.virustotal.com"))
        {
          wsprintf (params, vt_path, apikey, res);
          if (req_open (L"POST", params)) {
            req_dispatch ();
          }
          req_close();
        }
        http_close();
      }
    }
  }
  return 0;
}
