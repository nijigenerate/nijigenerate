#pragma once

#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::viewport::depth::mesheditor::node::detail {

// The real formatter still runs before the original empty log function.
template<class... Args> std::string formatEditorLog(std::string_view pattern, Args&&... args) {
    const std::array<core::translation::TypedFormatArgument,sizeof...(Args)> values{
        core::translation::ngFormatArgument(std::forward<Args>(args))...};
    return core::translation::ngFormatTyped(pattern,values);
}
inline std::string topologyInvalid(std::size_t vertices,std::size_t xs,std::size_t ys,std::size_t product) {
    return formatEditorLog("topology skipped: invalid grid vertices vertices=%s xs=%s ys=%s xs*ys=%s",
        vertices,xs,ys,product);
}
inline std::string topologyMissing(std::size_t x,std::size_t y) {
    return formatEditorLog("topology skipped cell: missing grid vertex at x=%s y=%s",x,y);
}
inline std::string topologyReady(std::size_t vertices,std::size_t xs,std::size_t ys,std::size_t triangles) {
    return formatEditorLog("topology ready: vertices=%s xs=%s ys=%s triangles=%s",vertices,xs,ys,triangles);
}
inline std::string depthsInitialized(std::int64_t sourceLength,std::size_t targetVertices) {
    return formatEditorLog("depths initialized: sourceLength=%s targetVertices=%s",sourceLength,targetVertices);
}
inline std::string depthsLoaded(std::size_t length) {return formatEditorLog("depths loaded: length=%s",length);}

} // namespace nijigenerate::viewport::depth::mesheditor::node::detail
