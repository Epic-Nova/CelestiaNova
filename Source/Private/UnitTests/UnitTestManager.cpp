#include "UnitTests/UnitTestManager.h"
#include "UnitTests/NovaFileOperationsTest.h"
#include "UnitTests/NovaFileOperationsExtendedTest.h"
#include "UnitTests/NovaLogTest.h"
#include "Core/NovaLog.h"

void UnitTestManager::Initialize(int argc, const char* argv[]) {
    // Parse command-line arguments for unit test flags
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--unit-test") {
            runUnitTest = true;
        } else if (arg.find("--unit-test-id=") == 0) {
            unitTestId = arg.substr(15); // Extract the ID after "--unit-test-id="
        }
    }
}

bool UnitTestManager::RunUnitTests() {
    if (!runUnitTest) {
        return false; // No unit test requested
    }

    // Unit test registry
    std::vector<std::unique_ptr<BaseUnitTest>> unitTests;
    unitTests.emplace_back(std::make_unique<NovaFileOperationsTest>());
    unitTests.emplace_back(std::make_unique<NovaFileOperationsExtendedTest>());
    unitTests.emplace_back(std::make_unique<NovaLogTest>());

    // Find and run the specified unit test
    for (const auto& test : unitTests) {
        if (test->GetIdentifier() == unitTestId) {
            bool success = test->Run();
            test->LogResult(success);
            return success;
        }
    }

    NOVA_LOG((std::string("Unit test with ID [") + unitTestId + "] not found.").c_str(), LogType::Error);
    return false;
}
