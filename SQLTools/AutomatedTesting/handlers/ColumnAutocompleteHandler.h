#pragma once
#include <string>
#include "AutomatedTesting/TestController.h"

class ColumnAutocompleteHandler : public ITestCommandHandler
{
public:
    bool Execute(const std::wstring& payload, std::wstring& out) override;
};
