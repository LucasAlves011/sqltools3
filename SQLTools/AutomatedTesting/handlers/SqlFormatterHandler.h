#pragma once
#include <string>
#include "AutomatedTesting/TestController.h"

class SqlFormatterHandler : public ITestCommandHandler
{
public:
	bool Execute(const std::wstring& payload, std::wstring& out) override;
};
