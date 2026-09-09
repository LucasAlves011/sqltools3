#include "StdAfx.h"
#include "SqlToolsProxy.h"
#include <objbase.h>
#include <initguid.h>
#include <atlbase.h>
#include <vector>

// Globals for class factory registration
static DWORD g_cfCookie = 0;
static IClassFactory* g_pFactory = nullptr;
static volatile LONG g_cObj = 0;
static volatile LONG g_cLock = 0;

static void MaybeShutdown()
{
    if (g_cObj == 0 && g_cLock == 0)
    {
        PostQuitMessage(0);
    }
}

// Hidden window per-thread storage
thread_local HWND ProxyAutomation::t_hiddenWnd = nullptr;
thread_local UINT32 ProxyAutomation::t_corrId = 0;
thread_local std::wstring* ProxyAutomation::t_pResult = nullptr;
thread_local bool ProxyAutomation::t_gotResponse = false;

static const wchar_t* kHiddenWndClass = L"SQLTools.Proxy.HiddenWnd";

// Registration helpers (minimal: CLSID + LocalServer32)
static std::wstring GetModulePath()
{
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf);
}

HRESULT ProxyRegisterServer()
{
    HKEY hKeyCLSID = nullptr;
    LSTATUS st = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"CLSID\\{A7E8F8B0-0FDD-4B37-9F61-6D4F3F8FCE21}",
                                 0, nullptr, 0, KEY_SET_VALUE|KEY_CREATE_SUB_KEY, nullptr, &hKeyCLSID, nullptr);
    if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);

    const wchar_t* desc = L"SQLTools Proxy COM Server";
    RegSetValueExW(hKeyCLSID, nullptr, 0, REG_SZ, (const BYTE*)desc, (DWORD)((wcslen(desc)+1)*sizeof(wchar_t)));

    HKEY hKeyLS = nullptr;
    st = RegCreateKeyExW(hKeyCLSID, L"LocalServer32", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKeyLS, nullptr);
    if (st == ERROR_SUCCESS)
    {
        std::wstring path = GetModulePath();
        RegSetValueExW(hKeyLS, nullptr, 0, REG_SZ, (const BYTE*)path.c_str(), (DWORD)((path.size()+1)*sizeof(wchar_t)));
        RegCloseKey(hKeyLS);
    }
    RegCloseKey(hKeyCLSID);

    // ProgID
    HKEY hKeyProg = nullptr;
    st = RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SQLTools.Proxy\\CLSID", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKeyProg, nullptr);
    if (st == ERROR_SUCCESS)
    {
        const wchar_t* clsid = L"{A7E8F8B0-0FDD-4B37-9F61-6D4F3F8FCE21}";
        RegSetValueExW(hKeyProg, nullptr, 0, REG_SZ, (const BYTE*)clsid, (DWORD)((wcslen(clsid)+1)*sizeof(wchar_t)));
        RegCloseKey(hKeyProg);
    }
    return S_OK;
}

HRESULT ProxyUnregisterServer()
{
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"SQLTools.Proxy");
    RegDeleteTreeW(HKEY_CLASSES_ROOT, L"CLSID\\{A7E8F8B0-0FDD-4B37-9F61-6D4F3F8FCE21}");
    return S_OK;
}

// ProxyAutomation implementation
ProxyAutomation::ProxyAutomation() : m_ref(1), m_typeInfo(nullptr)
{
    InterlockedIncrement(&g_cObj);
    CoAddRefServerProcess();
}
ProxyAutomation::~ProxyAutomation()
{
    if (m_typeInfo) m_typeInfo->Release();
    if (InterlockedDecrement(&g_cObj) == 0)
    {
        CoReleaseServerProcess();
        MaybeShutdown();
    }
}

STDMETHODIMP ProxyAutomation::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDispatch)
    {
        *ppv = static_cast<IDispatch*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) ProxyAutomation::AddRef() { return InterlockedIncrement(&m_ref); }
STDMETHODIMP_(ULONG) ProxyAutomation::Release()
{
    ULONG ul = InterlockedDecrement(&m_ref);
    if (!ul) delete this;
    return ul;
}

HRESULT ProxyAutomation::EnsureTypeInfo(LCID lcid)
{
    if (m_typeInfo) return S_OK;
    PARAMDATA param = { const_cast<LPOLESTR>(L"sqlWithPlaceholder"), VT_BSTR };
    METHODDATA meths[2] = {};
    meths[0].szName = const_cast<LPOLESTR>(L"TestColumnAutocompletion");
    meths[0].ppdata = &param;
    meths[0].dispid = 1;
    meths[0].iMeth = 0;
    meths[0].cc = CC_STDCALL;
    meths[0].cArgs = 1;
    meths[0].wFlags = DISPATCH_METHOD;
    meths[0].vtReturn = VT_BSTR;
    PARAMDATA param2 = { const_cast<LPOLESTR>(L"sqlToFormat"), VT_BSTR };
    meths[1].szName = const_cast<LPOLESTR>(L"TestSqlFormatter");
    meths[1].ppdata = &param2;
    meths[1].dispid = 2;
    meths[1].iMeth = 1;
    meths[1].cc = CC_STDCALL;
    meths[1].cArgs = 1;
    meths[1].wFlags = DISPATCH_METHOD;
    meths[1].vtReturn = VT_BSTR;
    INTERFACEDATA idata = { meths, 2 };
    return CreateDispTypeInfo(&idata, lcid, &m_typeInfo);
}

STDMETHODIMP ProxyAutomation::GetTypeInfoCount(UINT* pctinfo)
{ 
    if (pctinfo) *pctinfo = 0/*1*/; 
    return S_OK; 
}

STDMETHODIMP ProxyAutomation::GetTypeInfo(UINT, LCID lcid, ITypeInfo** ppTInfo)
{
    return E_NOTIMPL;
    //if (!ppTInfo) return E_POINTER;
    //HRESULT hr = EnsureTypeInfo(lcid);
    //if (FAILED(hr)) return hr;
    //m_typeInfo->AddRef();
    //*ppTInfo = m_typeInfo;
    //return S_OK;
}
STDMETHODIMP ProxyAutomation::GetIDsOfNames(REFIID, LPOLESTR* rgszNames, UINT cNames, LCID, DISPID* rgDispId)
{
    for (UINT i = 0; i < cNames; ++i)
    {
        if (_wcsicmp(rgszNames[i], L"TestColumnAutocompletion") == 0) { rgDispId[i] = 1; continue; }
        if (_wcsicmp(rgszNames[i], L"TestSqlFormatter") == 0) { rgDispId[i] = 2; continue; }
        return DISP_E_UNKNOWNNAME;
    }
    return S_OK;
}

ATOM ProxyAutomation::RegisterHiddenClass(HINSTANCE hInst)
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = ProxyAutomation::WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kHiddenWndClass;
    ATOM a = RegisterClassW(&wc);
    if (a == 0 && GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
        a = 1;
    return a;
}

HWND ProxyAutomation::EnsureHiddenWindow()
{
    if (t_hiddenWnd && IsWindow(t_hiddenWnd)) return t_hiddenWnd;
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    RegisterHiddenClass(hInst);
    t_hiddenWnd = CreateWindowW(kHiddenWndClass, L"", 0, 0,0,0,0, HWND_MESSAGE, nullptr, hInst, nullptr);
    return t_hiddenWnd;
}

LRESULT CALLBACK ProxyAutomation::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_COPYDATA)
    {
        COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (cds && cds->dwData == STP_COPYDATA_RESP && cds->lpData && cds->cbData >= sizeof(UINT32))
        {
            const BYTE* p = reinterpret_cast<const BYTE*>(cds->lpData);
            UINT32 corr = *reinterpret_cast<const UINT32*>(p);
            if (corr == t_corrId && t_pResult)
            {
                const wchar_t* s = reinterpret_cast<const wchar_t*>(p + sizeof(UINT32));
                if (s)
                    *t_pResult = s;
                else
                    t_pResult->clear();
                t_gotResponse = true;
                // Wake waiting thread
                PostMessageW(hWnd, WM_APP+1, 0, 0);
                return TRUE;
            }
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HRESULT ProxyAutomation::SendRequestToSqlTools(UINT cmd, const std::wstring& payload, std::wstring& outResult, DWORD timeoutMs)
{
    HWND hRecv = EnsureHiddenWindow();
    if (!hRecv) return E_FAIL;

    // Find SQLTools main frame by class name
    HWND hTarget = FindWindowW(L"Kochware.SQLTools.MainFrame", nullptr);
    if (!hTarget) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    // build request
    UINT32 corr = GetTickCount();
    t_corrId = corr;
    t_pResult = &outResult;
    t_gotResponse = false;

    const size_t cb = sizeof(UINT32)*2 + sizeof(HWND) + (payload.size()+1)*sizeof(wchar_t);
    std::vector<BYTE> buf(cb);
    BYTE* p = buf.data();
    *reinterpret_cast<UINT32*>(p) = cmd; p += sizeof(UINT32);
    *reinterpret_cast<UINT32*>(p) = corr; p += sizeof(UINT32);
    *reinterpret_cast<HWND*>(p) = hRecv; p += sizeof(HWND);
    memcpy(p, payload.c_str(), (payload.size()+1)*sizeof(wchar_t));

    COPYDATASTRUCT cds{};
    cds.dwData = STP_COPYDATA_REQ;
    cds.cbData = (DWORD)cb;
    cds.lpData = buf.data();

    DWORD_PTR dwRes = 0;
    if (!SendMessageTimeoutW(hTarget, WM_COPYDATA, (WPARAM)hRecv, (LPARAM)&cds,
                             SMTO_BLOCK, timeoutMs, &dwRes))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Wait for WndProc to receive response. Simple pump loop with timeout.
    DWORD start = GetTickCount();
    MSG m;
    while (GetTickCount() - start < timeoutMs)
    {
        while (PeekMessageW(&m, hRecv, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
            if (t_gotResponse)
                return S_OK;
        }
        Sleep(10);
    }
    return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
}

STDMETHODIMP ProxyAutomation::Invoke(DISPID dispIdMember, REFIID, LCID, WORD wFlags,
    DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO*, UINT*)
{
    if (!(wFlags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
    if (dispIdMember == 1)
    {
        if (!pDispParams || pDispParams->cArgs != 1) return DISP_E_BADPARAMCOUNT;
        CComVariant v(pDispParams->rgvarg[0]);
        HRESULT hr = v.ChangeType(VT_BSTR);
        if (FAILED(hr)) return DISP_E_TYPEMISMATCH;
        std::wstring payload(v.bstrVal ? v.bstrVal : L"");
        std::wstring result = L"echo:" + payload;

        hr = SendRequestToSqlTools(STP_CMD_TEST_COLUMN_AC, payload, result);
        if (FAILED(hr)) return hr;

        if (pVarResult)
        {
            VariantInit(pVarResult);
            pVarResult->vt = VT_BSTR;
            pVarResult->bstrVal = SysAllocString(result.c_str());
        }
        return S_OK;
    }
    if (dispIdMember == 2)
    {
        if (!pDispParams || pDispParams->cArgs != 1) return DISP_E_BADPARAMCOUNT;
        CComVariant v(pDispParams->rgvarg[0]);
        HRESULT hr = v.ChangeType(VT_BSTR);
        if (FAILED(hr)) return DISP_E_TYPEMISMATCH;
        std::wstring payload(v.bstrVal ? v.bstrVal : L"");
        std::wstring result;

        hr = SendRequestToSqlTools(STP_CMD_TEST_SQL_FORMATTER, payload, result);
        if (FAILED(hr)) return hr;

        if (pVarResult)
        {
            VariantInit(pVarResult);
            pVarResult->vt = VT_BSTR;
            pVarResult->bstrVal = SysAllocString(result.c_str());
        }
        return S_OK;
    }
    return DISP_E_MEMBERNOTFOUND;
}

// Factory
STDMETHODIMP ProxyClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr; return E_NOINTERFACE;
}
STDMETHODIMP_(ULONG) ProxyClassFactory::AddRef() { return InterlockedIncrement(&m_ref); }
STDMETHODIMP_(ULONG) ProxyClassFactory::Release()
{
    ULONG ul = InterlockedDecrement(&m_ref);
    if (!ul) delete this;
    return ul;
}
STDMETHODIMP ProxyClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    if (!ppv) return E_POINTER;
    ProxyAutomation* p = new (std::nothrow) ProxyAutomation();
    if (!p) return E_OUTOFMEMORY;
    HRESULT hr = p->QueryInterface(riid, ppv);
    p->Release();
    return hr;
}
STDMETHODIMP ProxyClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
    {
        InterlockedIncrement(&g_cLock);
        CoAddRefServerProcess();
    }
    else
    {
        if (InterlockedDecrement(&g_cLock) == 0)
        {
            CoReleaseServerProcess();
            MaybeShutdown();
        }
    }
    return S_OK;
}

// Entry point for EXE COM server
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmd, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return (int)hr;

    int ret = 0;
    if (cmd && (wcsstr(cmd, L"/RegServer") || wcsstr(cmd, L"-RegServer")))
    {
        ret = FAILED(ProxyRegisterServer()) ? 1 : 0;
    }
    else if (cmd && (wcsstr(cmd, L"/UnregServer") || wcsstr(cmd, L"-UnregServer")))
    {
        ret = FAILED(ProxyUnregisterServer()) ? 1 : 0;
    }
    else
    {
        ProxyClassFactory* pCF = new (std::nothrow) ProxyClassFactory();
        if (!pCF) { CoUninitialize(); return E_OUTOFMEMORY; }
        g_pFactory = pCF;
        hr = CoRegisterClassObject(CLSID_SqlToolsProxy, g_pFactory, CLSCTX_LOCAL_SERVER,
                                   REGCLS_MULTIPLEUSE, &g_cfCookie);
        if (SUCCEEDED(hr))
        {
            // Message loop
            MSG m; BOOL b;
            while ((b = GetMessageW(&m, nullptr, 0, 0)) != 0)
            {
                if (b == -1) break;
                TranslateMessage(&m);
                DispatchMessageW(&m);
            }
            CoRevokeClassObject(g_cfCookie); g_cfCookie = 0;
        }
        g_pFactory->Release(); g_pFactory = nullptr;
    }
    CoUninitialize();
    return ret;
}
