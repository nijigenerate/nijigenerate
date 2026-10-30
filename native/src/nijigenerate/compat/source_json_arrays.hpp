#pragma once
#include "slice.hpp"
#include <serialization_fields.hpp>

namespace nicxlive::core::serde {

template<JsonArrayElement T> struct SourceJsonArray<::nijigenerate::compat::Slice<T>> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::compat::Slice<T>& value, Serializer& serializer) {
        auto retained = value;
        if (retained.isNull()) { serializer.putValue(nullptr); return; }
        auto state = serializer.listBegin();
        const auto length = retained.size();
        for (std::size_t index = 0; index < length; ++index) {
            auto& element = retained[index];
            serializer.elemBegin();
            serializer.serializeValue(element);
        }
        serializer.listEnd(state);
    }
};

} // namespace nicxlive::core::serde
