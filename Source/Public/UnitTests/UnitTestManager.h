#ifndef UNIT_TEST_MANAGER_H
#define UNIT_TEST_MANAGER_H

#include "BaseUnitTest.h"
#include <vector>
#include <memory>
#include <string>
#include "Core/ModuleAPI.h"

class NOVA_CORE_API UnitTestManager {
public:
    void Initialize(int argc, const char* argv[]);
    bool RunUnitTests();

private:
    bool runUnitTest = false;
    std::string unitTestId;
};

#endif // UNIT_TEST_MANAGER_H
