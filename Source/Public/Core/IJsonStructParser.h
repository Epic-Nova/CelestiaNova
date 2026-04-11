#pragma once

#include <string>
#include <vector>

#include "Core/ModuleAPI.h"
#include "json.hpp"

namespace Core {

enum class EJsonParseSeverity {
    Info,
    Warning,
    Error
};

struct FJsonParseIssue {
    EJsonParseSeverity Severity = EJsonParseSeverity::Error;
    std::string JsonPath;
    std::string Message;
};

// Non-template base for shared validation contracts.
class NOVA_CORE_API IJsonSchemaValidator {
public:
    virtual ~IJsonSchemaValidator();

    virtual std::string GetSchemaId() const = 0;
    virtual bool Validate(const nlohmann::json& root,
                          std::vector<FJsonParseIssue>& outIssues) const = 0;
};

// Typed parser interface for converting JSON payloads into strongly typed structs.
// CanvasCore and other systems can implement this in their own modules while
// reusing a common Core-owned contract.
template <typename TStruct>
class IJsonStructParser : public IJsonSchemaValidator {
public:
    virtual ~IJsonStructParser() = default;

    virtual bool Parse(const nlohmann::json& root,
                       TStruct& outValue,
                       std::vector<FJsonParseIssue>& outIssues) const = 0;
};

} // namespace Core
