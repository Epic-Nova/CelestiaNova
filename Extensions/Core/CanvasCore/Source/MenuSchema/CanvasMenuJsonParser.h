#pragma once

#include <string>
#include <vector>

#include "Core/IJsonStructParser.h"
#include "MenuSchema/CanvasMenuSchemaProposal.h"

namespace CanvasCore::MenuSchema {

class CanvasMenuJsonParser : public Core::IJsonStructParser<FCanvasMenuFile> {
public:
    std::string GetSchemaId() const override;

    bool Validate(const nlohmann::json& root,
                  std::vector<Core::FJsonParseIssue>& outIssues) const override;

    bool Parse(const nlohmann::json& root,
               FCanvasMenuFile& outValue,
               std::vector<Core::FJsonParseIssue>& outIssues) const override;
};

} // namespace CanvasCore::MenuSchema
