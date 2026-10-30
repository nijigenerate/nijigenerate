#include "helpers_result.hpp"

namespace nijigenerate::api::mcp::helpers {
core::settings::SettingsJsonValue commandResultToJsonRuntime(std::shared_ptr<commands::CommandResult> result) {
    using namespace commands;
    using Json=core::settings::SettingsJsonValue;
    if(auto json=std::dynamic_pointer_cast<ExCommandResultImpl<Json>>(result)) return commandResultToJson(json);
    // Existing command providers with the same original JSONValue use the explicit Boost bridge.
    if(auto json=std::dynamic_pointer_cast<ExCommandResultImpl<boost::json::value>>(result)) return commandResultToJson(json);
    std::optional<Json> encoded;
    const auto resource=[&]<class T>() {
        if(encoded) return;
        using R=std::shared_ptr<T>;
        if(auto value=std::dynamic_pointer_cast<CreateResult<R>>(result)) encoded=commandResultToJson(value);
        else if(auto value=std::dynamic_pointer_cast<DeleteResult<R>>(result)) encoded=commandResultToJson(value);
        else if(auto value=std::dynamic_pointer_cast<LoadResult<R>>(result)) encoded=commandResultToJson(value);
    };
    resource.operator()<nicxlive::core::nodes::Node>();
    resource.operator()<nicxlive::core::param::Parameter>();
    resource.operator()<nicxlive::core::param::ParameterBinding>();
    resource.operator()<nicxlive::core::Puppet>();
    resource.operator()<ext::ExParameterGroup>();
    if(encoded) return *encoded;
    return commandResultToJson(result);
}
}
