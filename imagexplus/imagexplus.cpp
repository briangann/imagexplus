// imagexplus.cpp : Defines the entry point for the console application.
//
// AUTHOR
//     Brian Gann <bkgann@yahoo.com>
//
#include "stdafx.h"
#include <windows.h>
#include "wimgapi.h"
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <string>
#include <io.h>
#include <vector>
#include <strsafe.h>
#include <sys/types.h>  // For stat().
#include <sys/stat.h>   // For stat().

#define IN
#define OUT
#define INOUT

using namespace std;

typedef basic_string<TCHAR> tstring; // string of TCHARs

// globals
vector <std::wstring> excludedNames;
bool verboseOutput = false;

bool FileExists(TCHAR* filename) 
{
	bool exists = false;
	fstream fin;
	//wstring fs = filename;
	fin.open(filename,ios::in);
	if( fin.is_open() )
	{
		exists=true;
	}
	fin.close();
	return exists;
}

bool PathExists(TCHAR* path) 
{
	bool exists = false;
	int lenW = ::SysStringLen(path);
	char *ansistr = NULL;
	int lenA = ::WideCharToMultiByte(CP_ACP, 0, path, lenW, 0, 0, NULL, NULL);
	if (lenA > 0)
	{
		ansistr = new char[lenA + 1]; // allocate a final null terminator as well
		::WideCharToMultiByte(CP_ACP, 0, path, lenW, ansistr, lenA, NULL, NULL);
		ansistr[lenA] = 0; // Set the null terminator yourself
	}
	//cout << "Checking if path exists for: " << ansistr << endl;
	if (ansistr == NULL) return false;

	if (_access (ansistr, 0) == 0) 
	{
		struct stat status;
		stat (ansistr, &status);
		if ( status.st_mode & S_IFDIR ) 
		{
			exists = true;
		}
	}
	return exists;
}

void ShowLastError(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
	wcout << "Error: " << (LPTSTR)lpDisplayBuf << endl;

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}

void SetImageInformation(HANDLE hWIM, int imageNumber, TCHAR *name, TCHAR *description) {
	WCHAR *ImgInfoBuf, TempString[1500];
	DWORD ImgInfoBufSize;
	//wcout << "Getting info..." << endl;
	if (WIMGetImageInformation(hWIM,(LPVOID *)&ImgInfoBuf,&ImgInfoBufSize)) {
		// print it
		cout << "buffer size is " << ImgInfoBufSize << endl;
		StringCchPrintf(TempString, 1500, TEXT("<IMAGE INDEX=\"%d\">"), imageNumber);
		// look for the image index named above
		WCHAR *StringPointer = wcsstr(ImgInfoBuf, TempString);
		WCHAR *SecondString = wcsstr(++StringPointer, TEXT("<"));
		SecondString[-1] = 0;			//end the first string
		//Copy the first string to a new buffer and add our extra info
		WCHAR *NewInfoBuf = (WCHAR *)LocalAlloc(LPTR, (ImgInfoBufSize + 1500) * sizeof(WCHAR));
		WCHAR UnicodeFileMarker[] = TEXT("\uFEFF");
		//wcout << endl << "NEWINFO:" << endl << NewInfoBuf << ":" << endl;
		StringCchCopy(NewInfoBuf,wcslen(UnicodeFileMarker)*sizeof(WCHAR),UnicodeFileMarker);
		StringCchCat(NewInfoBuf,ImgInfoBufSize,ImgInfoBuf+1);
		if (name) {
			StringCchPrintf(TempString, 1500, TEXT(" <NAME>%s</NAME>\n"), name);
			StringCchCat(NewInfoBuf, 1500, TempString);
		}
		if (description) {
			StringCchPrintf(TempString, 1500, TEXT("    <DESCRIPTION>%s</DESCRIPTION>\n    "), description);
			StringCchCat(NewInfoBuf, 1500, TempString);
		}
		//Add the second string to the new buffer and reset the info in the file
		StringCchCat(NewInfoBuf, wcslen(SecondString) * sizeof(WCHAR), SecondString);
		// skip first char since it is not printable
		wcout << endl << "NEWINFO:" << endl << NewInfoBuf+1 << endl;
		//wcout << endl << "length:" << wcslen(NewInfoBuf) * sizeof(WCHAR) << endl;
		if (WIMSetImageInformation(hWIM, NewInfoBuf, (DWORD)wcslen(NewInfoBuf) * sizeof(WCHAR))) {
			wcout << "Updated WIM info" << endl;
		} else {
			ShowLastError(TEXT("SetImageInformation"));
		}
	} else { 
		ShowLastError(TEXT("SetImageInformation"));
	}
}

void ExportMetaData(TCHAR* wimFile, TCHAR* referenceFile, DWORD index, TCHAR *name, TCHAR *description) 
{
	wcout << "Exporting metadata to WIM  : " << wimFile;
    // call createfile to open existing reference file
    DWORD created = 0;
    // first, open a reference handle
    HANDLE hReferenceWIM = WIMCreateFile(
                           referenceFile,
                           WIM_GENERIC_READ,
                           WIM_OPEN_EXISTING,
                           WIM_FLAG_VERIFY,
                           0,
                           &created);
    // call loadimage on the reference file
    //wcout << "Setting Temp Paths" << endl;
    // set temp paths
    WIMSetTemporaryPath(hReferenceWIM,TEXT("C:\\"));
    HANDLE hLoadImage = WIMLoadImage(hReferenceWIM,index);
    WIMSetTemporaryPath(hLoadImage,TEXT("C:\\"));
    //export to .wim
    HANDLE hWIM = WIMCreateFile(
                           wimFile,
                           WIM_GENERIC_WRITE,
                           WIM_CREATE_NEW,
                           WIM_FLAG_VERIFY,
                           WIM_COMPRESS_LZX,
                           &created);
    WIMSetTemporaryPath(hWIM,TEXT("C:\\"));
    DWORD exportResult = WIMExportImage(hLoadImage,hWIM, WIM_EXPORT_ONLY_METADATA);

	// Store name and description
	// NOTE: this is simply inherited from the resource file now
	//SetImageInformation(hWIM,1,name,description);

	// close handles
    WIMCloseHandle(hLoadImage);
	WIMCloseHandle(hReferenceWIM);
	WIMCloseHandle(hWIM);
	cout << " (done)" << endl;
    cout << "Export Completed Successfully." << endl;
}




void CreateWIM(TCHAR* wimFile, TCHAR* referenceFile, TCHAR* pathToCapture, TCHAR *name, TCHAR *description, boolean append) 
{
	HANDLE hReferenceWIM = NULL;
	DWORD created = 0;
	DWORD flags = WIM_CREATE_NEW;
    if (append) {
        flags = WIM_OPEN_EXISTING;
    }
	hReferenceWIM = WIMCreateFile(
						referenceFile,
                        WIM_GENERIC_WRITE,
                        flags,
                        WIM_FLAG_VERIFY,
                        WIM_COMPRESS_LZX,
                        &created);

	if ( !hReferenceWIM ) 
	{
		wcout << "Cannot create/open WIM file\n";
		exit(-1);
	}
	// get the number of volumes inside this reference file
    DWORD numVolumes = 0;
    if (append) {
        numVolumes = WIMGetImageCount(hReferenceWIM);
        wcout << "  (There are " << numVolumes << " volumes in " << referenceFile << ")" << endl;
    }
	wcout << "Capturing to Reference file: " << referenceFile << endl;
    // Capture!
    HANDLE hCapture = WIMCaptureImage(
            hReferenceWIM,
            pathToCapture,
            WIM_FLAG_VERIFY);
	// Store image info
	SetImageInformation(hCapture,numVolumes+1,name,description);

    // close the capture handle
    WIMCloseHandle(hCapture);
    // close the create handle
    WIMCloseHandle(hReferenceWIM);
    // done!
    wcout << "Reference File Completed." << endl;
    // index is always 1 for a new wim/rwm pair
	numVolumes++;
    ExportMetaData(wimFile,referenceFile,numVolumes,name,description);
    cout << "Capture Completed Successfully." << endl;
}

void ShowUsage (TCHAR *progName)
{
	wcout << endl;
	wcout << "-----------------------------" << endl;
	wcout << "ImageXPlus (bkgann@yahoo.com)" << endl;
	wcout << "-----------------------------" << endl;
	wcout << "Usage:" << endl;
	wcout << "        ImageXPlus " << endl;
	wcout << "               [-h|--help]" << endl;
	wcout << "               [-v|--verbose]" << endl;
	wcout << "               [--name|-n] anyname" << endl;
	wcout << "               [--description|-d] description" << endl;
	wcout << "               [--config|-c] config.ini" << endl;
	wcout << "               [--reference|-r] filename.rwm" << endl;
	wcout << "               [--wim|-w] filename.wim" << endl;
	wcout << "               [--path|-p] C:\\foo" << endl;
	wcout << endl;
} 

void ReadConfigFile(TCHAR *filename) {
	string line;
	int inExclusions = 0;
	if (filename) {
		ifstream myfile (filename);
		if (myfile.is_open()) {
			while (! myfile.eof() ) {
				getline (myfile,line);
				//cout << "READ: " << line << ":" << endl;
				if (!line.empty()) {
					if (line.compare("[ExclusionList]") != 0) {
						if (inExclusions) {
							std::wstring widestr = std::wstring(line.begin(),line.end());
							excludedNames.push_back(widestr);
						}
					} else {
						inExclusions = 1;
					}
				}
			}
			myfile.close();
			// show exclusions
			for(unsigned int i=0;i < excludedNames.size(); i++) {
				std::wstring strd = excludedNames.at(i);
				wcout << "Excluding: " << strd.c_str()<<endl;
			}
		} else wcout << "Unable to open file " << filename << endl; 
	}
}


//
//Callback function:
//
DWORD
WINAPI
CaptureCallback(
    IN      DWORD msgId,    //message ID
    IN      WPARAM param1,   //usually file name
    INOUT   LPARAM param2,   //usually error code
    IN      void  *unused
    )
{
    //First parameter: full file path for if WIM_MSG_PROCESS, message string for others
    TCHAR *message  = (TCHAR *) param1;
    TCHAR *filePath = (TCHAR *) param1;
	std::wstring wFilePath; // = NULL;
	//wstring(filePath);
    DWORD percent   = (DWORD)   param1;

    //Second parameter: message back to caller if WIM_MSG_PROCESS, error code for others
    DWORD errorCode = (DWORD) param2;
    BOOL *msg_back = (BOOL *) param2;
	bool kept = true;


    switch ( msgId )
    {
        case WIM_MSG_PROCESS:

            //This message is sent for each file, capturing to see if callee intends to
            //capture the file or not.
            //
            //If you do not intend to capture this file, then assign FALSE in msg_back
            //and still return WIM_MSG_SUCCESS.
            //Default is TRUE.
            //

            //In this example, print out the file name being applied
            //
            //_tprintf(TEXT("FilePath: %s\n"), filePath);
			wFilePath = wstring(filePath);
			if (verboseOutput) {
				wcout << wFilePath;
			}
			for(unsigned int i=0;i < excludedNames.size(); i++) {
				std::wstring pattern = excludedNames.at(i);
				size_t found;
				//wcout << "Looking for " << pattern << " in " << wFilePath << endl;
				found = wFilePath.find(pattern);
				if (found != wstring::npos) {
					//wcout << "SKIPPED: matched pattern " << pattern << " in " << wFilePath << " at position " << int(found) << endl;
					*msg_back = FALSE;
					kept = false;
				}
			}
			if (!kept) {
				if (verboseOutput) {
					wcout << " (skipped)" << endl;
				}
			} else {
				if (verboseOutput) {
					wcout << " (kept)" << endl;
				}
			}
            break;
		case WIM_MSG_COMPRESS:
			//Set the compression exclusions
			if (wcsstr(filePath, TEXT(".mp3"))                        ||
				wcsstr(filePath, TEXT(".zip"))                        ||
				wcsstr(filePath, TEXT(".cab"))                        ||
				wcsstr(filePath, TEXT(".rar"))                        ||
				wcsstr(filePath, TEXT(".7z"))                         ||
				wcsstr(filePath, TEXT(".pnf"))                            )
					*msg_back = FALSE;
			break;
        case WIM_MSG_ERROR:

            //This message is sent upon failure error case
            //
            printf("ERROR: %s [err = %d]\n", message, errorCode);
            break;

        case WIM_MSG_RETRY:

            //This message is sent when the file is being reapplied because of
            //network timeout. Retry is done up to five times.
            //
            printf("RETRY: %s [err = %d]\n", message, errorCode);
            break;

        case WIM_MSG_INFO:

            //This message is sent when informational message is available
            //
            printf("INFO: %s [err = %d]\n", message, errorCode);
            break;

        case WIM_MSG_WARNING:

            //This message is sent when warning message is available
            //
            printf("WARNING: %s [err = %d]\n", message, errorCode);
            break;
    }

    return WIM_MSG_SUCCESS;
}



int wmain(DWORD argc, TCHAR *argv[])
{
	DWORD dwError = ERROR_SUCCESS;
	FARPROC callback = (FARPROC) CaptureCallback;
	// get the arguments
	DWORD argIndex = 1;
	TCHAR *wimFile = NULL;
	TCHAR *referenceFile = NULL;
	TCHAR *pathToCapture = NULL;
	TCHAR *configFile = NULL;
	TCHAR *name = NULL;
	TCHAR *description = NULL;
	// if no arguments given, show usage
	if (argc == 1) {
			ShowUsage(argv[0]);
			exit(0);
	}
	while ((argIndex < argc) && (argv[argIndex][0] == '-')) 
	{
		wstring sw = argv[argIndex];
		if ((sw == TEXT("--wim")) || (sw == TEXT("-w")))
		{
			argIndex++;
			wimFile = argv[argIndex];
		}
		else if ((sw == TEXT("--verbose")) || (sw == TEXT("-v")))
		{
			argIndex++;
			verboseOutput = true;
		}
		else if ((sw == TEXT("--config")) || (sw == TEXT("-c")))
		{
			argIndex++;
			configFile = argv[argIndex];
		}
		else if ((sw == TEXT("--name")) || (sw == TEXT("-n")))
		{
			argIndex++;
			name = argv[argIndex];
		}
		else if ((sw == TEXT("--description")) || (sw == TEXT("-d")))
		{
			argIndex++;
			description = argv[argIndex];
		}
		else if ((sw == TEXT("--reference")) || (sw == TEXT("-r")))
		{
			argIndex++;
			referenceFile = argv[argIndex];
		}
		else if ((sw == TEXT("--path")) || (sw == TEXT("-p")))
		{
			argIndex++;
			pathToCapture = argv[argIndex];
		}
		else if ((sw == TEXT("--help")) || (sw == TEXT("-h")))
		{
			ShowUsage(argv[0]);
			exit(0);
		}
		else {
			wcout << "Unknown argument: " << argv[argIndex] << endl;
			ShowUsage(argv[0]);
			exit(-1);
		}
		argIndex++;
	}

	if (configFile) {
		if (FileExists(configFile)) 
		{
			wcout << "Using config file: " << configFile << endl;
			// now read the config file
			ReadConfigFile(configFile);
		} else {
			wcout << "config file not found!" << endl;
			exit(-1);
		}
	} 
	//Register callback
    //
    if (WIMRegisterMessageCallback( NULL,
                                    callback,
                                    NULL ) == INVALID_CALLBACK_VALUE) {
        printf ("Cannot set callback\n");
        return 3;
    }

	//exit(0);
	wcout << endl;
	wcout << "-----------------------------" << endl;
	wcout << "ImageX2 (bgann@microsoft.com)" << endl;
	wcout << "-----------------------------" << endl;
	wcout << "WIM File         : " << wimFile;
	if (FileExists(wimFile)) 
	{
		wcout << " (EXISTS)" << endl;
	} else {
		wcout << endl;
	}
	wcout << "Reference File   : " << referenceFile;
	if (FileExists(referenceFile)) 
	{
		wcout << " (EXISTS)" << endl;
	} else {
		wcout << endl;
	}
	wcout << "Path To Capture  : " << pathToCapture;
	if (PathExists(pathToCapture)) 
	{
		wcout << " (EXISTS)" << endl;
	} else {
		wcout << endl << endl << "ERROR: Path does not exist!" << endl;
		exit(-1);
	}
	wcout << endl;
	if ( (!FileExists(referenceFile)) && (!FileExists(wimFile)) )
	{
		wcout << "Creating new WIM and storing files in new reference " << referenceFile << endl;
		CreateWIM(wimFile,referenceFile,pathToCapture,name,description,false);
		exit(0);
	}
	if ( (FileExists(referenceFile)) && (!FileExists(wimFile)))
	{
		wcout << "Creating new WIM and appending files to existing reference " << referenceFile << endl;
		CreateWIM(wimFile,referenceFile,pathToCapture,name,description,true);
		exit(0);
	}

	wcout << endl;
	wcout << "ERROR: ImageX2.exe can only create new wim/reference pairs, or append to an existing reference file." << endl;
	wcout << endl;
	ShowUsage(argv[0]);
}

