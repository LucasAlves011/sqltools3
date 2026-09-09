#include "stdafx.h"
#include "TestController.h"
#include <utility>

// forward declare built-in handlers registration
namespace TestingHandlers { void RegisterAll(TestController& ctl); }

TestController& TestController::Instance()
{
    static TestController s;
    s.EnsureRegistered();
    return s;
}

TestController::TestController() = default;

void TestController::EnsureRegistered()
{
    std::call_once(m_regOnce, [this]() {
        TestingHandlers::RegisterAll(*this);
    });
}

bool TestController::Handle(unsigned cmd, const std::wstring& payload, std::wstring& out)
{
    EnsureRegistered();
    auto it = m_handlers.find(cmd);
    if (it == m_handlers.end())
        return false;
    return it->second->Execute(payload, out);
}

void TestController::Register(unsigned cmd, std::unique_ptr<ITestCommandHandler> handler)
{
    m_handlers[cmd] = std::move(handler);
}

// ================= Handlers =================
#include "handlers/ColumnAutocompleteHandler.h"
#include "handlers/SqlFormatterHandler.h"

namespace TestingHandlers
{
    void RegisterAll(TestController& ctl)
    {
        ctl.Register(1 /*STP_CMD_TEST_COLUMN_AC*/, std::unique_ptr<ITestCommandHandler>(new ColumnAutocompleteHandler()));
        ctl.Register(2 /*STP_CMD_TEST_SQL_FORMATTER*/, std::unique_ptr<ITestCommandHandler>(new SqlFormatterHandler()));
    }
}
