#include	"stdafx.h"

#include	<string>
#include	<fstream>

#include	<CommCtrl.h>

#include	<Shlwapi.h>
#include	<ShlObj.h>

#pragma	comment(lib, "Shlwapi.lib")

//
//	�ж��Ƿ�64λϵͳ
//
bool	win_is_64bits_system(){
	typedef VOID (WINAPI *LPFN_GetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);

	SYSTEM_INFO si;
	LPFN_GetNativeSystemInfo fnGetNativeSystemInfo = (LPFN_GetNativeSystemInfo)GetProcAddress( GetModuleHandle(_T("kernel32")), "GetNativeSystemInfo");;
	if (NULL != fnGetNativeSystemInfo){
		fnGetNativeSystemInfo(&si);
	}else{
		GetSystemInfo(&si);
	}
	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
		si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ){
			return true;
	}

	return false;
}

//
//	����������ȡ�Ӵ��ڣ���һ����������� FindWindowEx ����
//
HWND	win_find_child_by_class(HWND hWnd, const char* sClass){
	HWND	hChild	= GetWindow(hWnd, GW_CHILD);
	if(NULL == hChild){
		return	hChild;
	}

	do{
		char	buf[MAX_PATH]	= {};
		GetClassName(hChild, buf, sizeof(buf) - 1);
		if(0 == _strcmpi(buf, sClass)){
			return	hChild;
		}

		hChild	= GetWindow(hChild, GW_HWNDNEXT);
	}while(hChild != NULL);

	return	NULL;
}

//
//	��ȡ����SysListView���ھ��
//
HWND	win_get_desktop_SysListView(){
	HWND	hWnd	= GetDesktopWindow();
	if(NULL != hWnd){
		hWnd	=	win_find_child_by_class(hWnd, "Progman");
		if(NULL == hWnd){
			hWnd	= FindWindow("Progman", NULL);
		}
	}
	if(NULL != hWnd){
		hWnd	=	win_find_child_by_class(hWnd, "SHELLDLL_DefView");
	}
	if(NULL != hWnd){
		hWnd	=	win_find_child_by_class(hWnd, "SysListView32");
	}
	return	hWnd;
}

//
//	��ȡ����ͼ�����
//
bool	win_get_desktop_icon_rect(const char* psCaption, RECT* pRect){
	HANDLE	hProcess	= NULL;
	HWND	hView		= win_get_desktop_SysListView();
	{
		DWORD PID;
		GetWindowThreadProcessId(hView, &PID);
		hProcess=OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, PID);
		if(NULL == hProcess){
			return	false;
		}
	}

	LVITEM	xItem,	*pRemoteItem		= NULL;
	char	sText[512],	*pRemoteText	= NULL;

	pRemoteItem	= (LVITEM*)	VirtualAllocEx(hProcess, NULL, sizeof(xItem), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	pRemoteText	= (char*)	VirtualAllocEx(hProcess, NULL, sizeof(sText), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	bool	bFound	= false;
	int		nSum	= ListView_GetItemCount(hView);
	for(int i = 0; i < nSum; ++i){
		memset(&xItem, 0, sizeof(xItem));
		xItem.mask			= LVIF_TEXT;
		xItem.iItem			= i;
		xItem.iSubItem		= 0;
		xItem.pszText		= pRemoteText;
		xItem.cchTextMax	= sizeof(sText);

		WriteProcessMemory(hProcess, pRemoteItem, &xItem, sizeof(xItem), NULL);
		::SendMessage(hView, LVM_GETITEM, 0, (LPARAM)pRemoteItem);
		ReadProcessMemory(hProcess, pRemoteText, sText, sizeof(sText), NULL);
		if(0 == _strcmpi(sText, psCaption)){
			if(NULL != pRect){
				memset(pRect, 0, sizeof(RECT));
				pRect->left	= LVIR_SELECTBOUNDS;
				WriteProcessMemory(hProcess, pRemoteText, pRect, sizeof(RECT), NULL);
				::SendMessage(hView, LVM_GETITEMRECT, (WPARAM)i, (LPARAM)pRemoteText);
				ReadProcessMemory(hProcess, pRemoteText, pRect, sizeof(RECT), NULL);
			}
			bFound	= true;
			break;
		}
	}

	CloseHandle(hProcess);
	VirtualFreeEx(hProcess, pRemoteItem, 0, MEM_RELEASE);
	VirtualFreeEx(hProcess, pRemoteText, 0, MEM_RELEASE);

	return	bFound;
}

//
//	��ȡ��Դ����
//
bool	win_load_resource_data(
    HMODULE		hModule,
    const char*	res_name,
    const char*	res_type,
	void*&		res_data,
	size_t&		res_size
	){
	HRSRC hRsrc		= FindResourceA(hModule, res_name, res_type);
	if(NULL == hRsrc)	return false;

	DWORD dwSize	= SizeofResource(hModule, hRsrc);
	if(0 == dwSize)		return false;

	HGLOBAL hGlobal	= LoadResource(hModule, hRsrc);
	if(NULL == hGlobal)	return false;

	LPVOID pBuffer	= LockResource(hGlobal);
	if(NULL == pBuffer)	return false;

	res_data	=	pBuffer;
	res_size	=	dwSize;

	return	true;
}

//
//	��ȡ�ļ�����(����ȷ���ռ��㹻��)
//
bool	win_load_file_data(
    const char*	file_name,
	void*		res_data,
	size_t&		res_size
	){
	std::ifstream	ifs(file_name, std::ios::binary);
	if(!ifs){
		return	false;
	}

	ifs.seekg(0, std::ios::end);
	size_t	file_size	= size_t(ifs.tellg());
	if(NULL == res_data){
		res_size	= file_size;
		return	true;
	}

	if(res_size < file_size){
		return	false;
	}

	res_size	= file_size;
	ifs.seekg(0, std::ios::beg);
	ifs.read((char*)res_data, res_size);

	return	true;
}

//
//	��ȡHBITMAP��С
//
bool	win_get_bitmap_size(HBITMAP hbmp, long& nWidth, long& nHeight){
	BITMAP bmp;
	if(NULL == hbmp || !::GetObject(hbmp, sizeof(BITMAP), &bmp)){
		return false;
	}

	nWidth	= bmp.bmWidth;
	nHeight	= bmp.bmHeight;
	return true;
}

//
//	��ȡ��ǰӦ�ø�Ŀ¼�������ָ���/��
//
std::string	win_get_root_path(){
	char szPath[MAX_PATH] = {0};
	GetModuleFileName(GetModuleHandle(NULL), szPath, MAX_PATH);
	PathRemoveFileSpec(szPath);
	PathAddBackslash(szPath);
	return	std::string(szPath);
}

//
//	������ݷ�ʽ 
//
bool	win_create_shortcut(const char* szPath, const char* szWorkingPath, const char* szLink){ 
	HRESULT hres ; 
	IShellLink * psl ; 
	IPersistFile* ppf ; 
	WCHAR wsz[MAX_PATH]={L""}; 
	////��ʼ��COM 
	CoInitialize (NULL); 
	//����һ��IShellLinkʵ�� 
	hres = CoCreateInstance( CLSID_ShellLink, NULL,CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);

	if( FAILED( hres)) 
	{ 
		CoUninitialize (); 
		return false ; 
	} 

	//����Ŀ��Ӧ�ó��� 
	psl->SetPath( szPath);
	psl->SetWorkingDirectory(szWorkingPath); 

	//��IShellLink��ȡ��IPersistFile�ӿ� 
	//���ڱ����ݷ�ʽ�������ļ� (*.lnk) 
	hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf) ; 
	if( FAILED( hres)) 
	{ 
		CoUninitialize (); 
		return false ; 
	} 

	// ȷ�������ļ���ΪANSI��ʽ 
	MultiByteToWideChar( CP_ACP, 0, szLink, -1, wsz, MAX_PATH) ; 

	//����IPersistFile::Save 
	//�����ݷ�ʽ�������ļ� (*.lnk) 
	hres = ppf->Save(wsz, STGM_READWRITE); 
	//�ͷ�IPersistFile��IShellLink�ӿ� 
	ppf->Release() ; 
	psl->Release() ; 
	CoUninitialize();

	return true; 
}