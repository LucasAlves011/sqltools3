#pragma once
#include <string>
#include <map>
#include <memory>
#include <mutex>

// Simple testing command controller that routes commands from proxy to handlers.
// Commands use UINT identifiers. Payload/result are UTF-16 strings.

struct ITestCommandHandler
{
    virtual ~ITestCommandHandler() = default;
    virtual bool Execute(const std::wstring& payload, std::wstring& out) = 0;
};

class TestController
{
public:
    static TestController& Instance();

    // Dispatch a command; returns false if not found or on handler failure.
    bool Handle(unsigned cmd, const std::wstring& payload, std::wstring& out);

    // Allow future dynamic registration if needed
    void Register(unsigned cmd, std::unique_ptr<ITestCommandHandler> handler);

private:
    TestController();
    void EnsureRegistered();

    std::once_flag m_regOnce;
    std::map<unsigned, std::unique_ptr<ITestCommandHandler>> m_handlers;
};
