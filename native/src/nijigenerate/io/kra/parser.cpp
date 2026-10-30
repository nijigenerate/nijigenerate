// Independent kra-d parser.d document and XML import bodies. BSD-2-Clause.
#include "parser_detail.hpp"
#include "zip.hpp"
#include "xml.hpp"
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <compat/phobos_file_read.hpp>

namespace nijigenerate::io::kra {
namespace {
using Entity = xml::Entity;
using Attributes = compat::Slice<std::array<compat::RetainedString, 2>>;
[[noreturn]] void failDocument(const char* message, std::size_t line) {
    throw compat::ThrowableException{compat::RetainedString::fromBorrowedView(message),
        compat::RetainedString::fromStaticLiteral("kra/parser.d"), line};
}
compat::RetainedString getAttribute(Attributes attributes, std::string_view name) {
    // Original filter.front precedes the fallback expression: absence is an error.
    for (const auto& attribute : attributes) if (attribute[0].view() == name) return attribute[1];
#ifndef NDEBUG
    throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Attempting to fetch the front of an empty filter."),
        compat::RetainedString::fromStaticLiteral("std/algorithm/iteration.d"), 1384);
#else
    throw compat::ngStaticError<compat::ArrayIndexError>(0, 0,
        compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2610);
#endif
}
std::int32_t intAttribute(Attributes attributes, std::string_view name) {
    return detail::integer(getAttribute(attributes, name).view());
}
std::string buildPathKRA(compat::RetainedString root, std::string_view directory, compat::RetainedString leaf) {
    return std::string(root.view()) + "/" + std::string(directory) + "/" + std::string(leaf.view());
}
Layer baseLayer(Attributes attributes) {
    Layer layer;
    layer.name = getAttribute(attributes, "name");
    layer.isVisible = intAttribute(attributes, "visible") != 0;
    layer.opacity = intAttribute(attributes, "opacity"); layer.uuid = getAttribute(attributes, "uuid");
    layer.x = intAttribute(attributes, "x"); layer.y = intAttribute(attributes, "y");
    return layer;
}
void importAttributes(KRA& kra, const std::shared_ptr<Entity>& entity) {
    const auto children = entity->children;
    for (const auto child : children) {
        if (child->name.view() != "layer") continue;
        const auto attributes = child->attributes;
        const auto type = getAttribute(attributes, "nodetype");
        if (type.view() == "paintlayer") {
            auto layer = baseLayer(attributes);
            const auto color = getAttribute(attributes, "colorspacename");
            const auto file = getAttribute(attributes, "filename");
            if (file.empty()) failDocument("Invalid layer: filename attribute is required", 124);
            const auto composite = getAttribute(attributes, "compositeop");
            layer.type = LayerType::Any; layer.blendModeKey.value = composite; layer.colorMode.value = color;
            const auto entryName = buildPathKRA(kra.name, "layers", file);
            const auto found = kra.fileRef->directory.find(entryName);
            if (found == kra.fileRef->directory.end())
                throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("kra/parser.d"), 132);
            const auto entry = found->second;
            kra.fileRef->expand(entry);
            if (detail::parseLayerData(entry->expandedData, layer)) kra.layers.append(layer);
        } else if (type.view() == "grouplayer") {
            const bool collapsed = intAttribute(attributes, "collapsed") != 0;
            auto group = baseLayer(attributes);
            group.type = collapsed ? LayerType::ClosedFolder : LayerType::OpenFolder;
            kra.layers.append(group); importAttributes(kra, child->children[0]);
            Layer end; end.type = LayerType::SectionDivider; kra.layers.append(end);
        }
    }
}
KRA parseKRAFile(std::shared_ptr<ZipArchive> file) {
    KRA kra; kra.fileRef = file;
    if (!file->directory.contains("mimetype")) failDocument("Invalid documento: no file 'mimetype'", 45);
    const auto mime = file->directory.at("mimetype"); file->expand(mime);
    const std::string_view mimeText(reinterpret_cast<const char*>(mime->expandedData.data()), mime->expandedData.size());
    if (mimeText != "application/x-krita") failDocument("Invalid document: invalid mimetype.", 50);
    if (!file->directory.contains("maindoc.xml")) failDocument("Invalid document: no file 'maindoc.xml", 53);
    const auto main = file->directory.at("maindoc.xml"); file->expand(main);
    const auto dom = xml::parseSimpleXML(compat::RetainedString(std::string_view(
        reinterpret_cast<const char*>(main->expandedData.data()), main->expandedData.size())));
    const auto document = dom->children[0]; const auto image = document->children[0];
    const auto attributes = image->attributes;
    kra.width = intAttribute(attributes, "width"); kra.height = intAttribute(attributes, "height");
    kra.name = getAttribute(attributes, "name");
    const auto color = getAttribute(attributes, "colorspacename");
    if (color.view() != "RGBA") failDocument("Support for 8-bit Integer documents only.", 70);
    kra.colorMode.value = color;
    importAttributes(kra, image->children[0]);
    return kra;
}
}
KRA parseDocument(compat::RetainedString name) {
    auto contents = nicxlive::core::d_runtime::ngReadBinaryFile(std::move(name));
    const auto bytes = compat::Slice<std::uint8_t>::retainedView(
        contents.retentionOwner(), contents.data(), contents.size());
    return parseKRAFile(std::make_shared<ZipArchive>(bytes));
}
}
