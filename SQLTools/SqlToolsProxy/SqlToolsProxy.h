#pragma once
#include <windows.h>
#include <oaidl.h>
#include <string>

// CLSID for SqlToolsProxy: {A7E8F8B0-0FDD-4B37-9F61-6D4F3F8FCE21}
// ProgID: SQLTools.Proxy
// Update registration if you change these.
DEFINE_GUID(CLSID_SqlToolsProxy,
0xa7e8f8b0, 0x0fdd, 0x4b37, 0x9f, 0x61, 0x6d, 0x4f, 0x3f, 0x8f, 0xce, 0x21);

// COPYDATA protocol constants (must match SQLTools main frame)
static const DWORD STP_COPYDATA_REQ  = 0x52505453; // 'STPR'
static const DWORD STP_COPYDATA_RESP = 0x53505453; // 'STPS'
static const UINT  STP_CMD_TEST_COLUMN_AC = 1;     // TestColumnAutocompletion
static const UINT  STP_CMD_TEST_SQL_FORMATTER = 2;    // TestSqlFormatter

// Utility to write registry entries for COM registration
HRESULT ProxyRegisterServer();
HRESULT ProxyUnregisterServer();

// IDispatch-based COM object exposed by the proxy
class ProxyAutomation : public IDispatch
{
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override;
    STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override;
    STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                               LCID lcid, DISPID* rgDispId) override;
    STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                        DISPPARAMS* pDispParams, VARIANT* pVarResult,
                        EXCEPINFO* pExcepInfo, UINT* puArgErr) override;

    // ctor/dtor
    ProxyAutomation();
    virtual ~ProxyAutomation();

private:
    LONG m_ref;
    ITypeInfo* m_typeInfo; // cached runtime type info

    HRESULT EnsureTypeInfo(LCID lcid);

    // Messaging to SQLTools main frame
    HRESULT SendRequestToSqlTools(UINT cmd, const std::wstring& payload, std::wstring& outResult, DWORD timeoutMs = 10000);

    // Hidden window state for receiving replies
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static ATOM RegisterHiddenClass(HINSTANCE hInst);
    HWND  EnsureHiddenWindow();
    static thread_local HWND t_hiddenWnd; // per-thread receiver
    static thread_local UINT32 t_corrId;
    static thread_local std::wstring* t_pResult;
    static thread_local bool t_gotResponse;
};

// COM class factory
class ProxyClassFactory : public IClassFactory
{
public:
    ProxyClassFactory() : m_ref(1) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    LONG m_ref;
};
