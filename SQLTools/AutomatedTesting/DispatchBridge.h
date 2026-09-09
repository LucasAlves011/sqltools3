#pragma once
#include <string>
#include "AutomatedTesting/TestController.h"

inline bool AutomatedTesting_Dispatch(unsigned cmd, const std::wstring& payload, std::wstring& out)
{
    return TestController::Instance().Handle(cmd, payload, out);
}
