#pragma once
#include "mir_rc_context.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace nijigenerate::compat {
// The defining Source type supplies this specialization with actual bodies:
// sourceByteSize, elaborateAssign, elaborateDestructor, initializerPayload(),
// fillDefault(T*, size_t), emplaceInt(T*, int32_t), assignElement(T&, T&), and
// destroyElement(T&) when the Source elaborate-destructor flag is true.
// Optional noMutableIndirections is a Source declaration fact, not a native
// triviality trait; it admits the separate plain-value qualification adapter.
// Missing Source definitions stay missing; native type names, constructors and
// destructibility do not manufacture these properties or callbacks.
template<class T> struct MirRCElementDefinition;
template<class T> class MirRCArray;
template<class T, class Enable> struct MirRCElementOperations;

struct MirRCRequiresSourceIntBody {};
struct MirRCInvalidSourceIntBody {};
// The default is a mandatory body lookup, not a positive feasibility claim.
// A proven Source-negative definition deletes emplace and records that case;
// callers then diagnose it without inventing unused size/default/destructor data.
template<class T> struct MirRCIntConstruction {
    using SourceCase = MirRCRequiresSourceIntBody;
    static void emplace(T* destination, std::int32_t value) {
        MirRCElementOperations<std::remove_cv_t<T>, void>::emplaceInt(destination, value);
    }
};

// A deep Source const view is supplied by the defining type's actual projection,
// not inferred from native top-level const. Plain declared values opt into the
// separately implemented same-context projection below the raw array carrier.
template<class T, class = void> struct MirRCConstProjection {
    using Definition = MirRCElementDefinition<std::remove_cv_t<T>>;
    static auto retain(const MirRCArray<T>& value) {
        return Definition::retainSourceConstArray(value);
    }
    static auto move(MirRCArray<T>& value) {
        return Definition::moveSourceConstArray(value);
    }
};
template<class T> struct MirRCPlainConstProjection;

template<class T, class Definition = MirRCElementDefinition<T>>
struct MirRCDefinedElementOperations {
    // This guards the current raw-payload carrier against memory corruption.
    // A different native representation needs its own actual storage provider;
    // native sizeof does not establish the original Source type's byte size.
    static_assert(sizeof(T) == static_cast<std::size_t>(Definition::sourceByteSize),
        "MIR raw payload requires a Source-size-compatible mapped carrier");
    inline static constexpr bool elaborateAssign = Definition::elaborateAssign;
    inline static constexpr bool elaborateDestructor = Definition::elaborateDestructor;
private:
    static void destroy(void* element) {
        // The real Source destructor body is not a guessed native destructor.
        Definition::destroyElement(*static_cast<T*>(element));
    }
    static constexpr auto sourceDestructor() -> void (*)(void*) {
        if constexpr (elaborateDestructor) return &destroy;
        else return nullptr;
    }
public:
    inline static const MirRCTypeInfo typeInfo{sourceDestructor(), Definition::sourceByteSize};
    static const void* payload() { return Definition::initializerPayload(); }
    static void fillDefault(T* elements, std::size_t length) {
        Definition::fillDefault(elements, length);
    }
    static void emplaceInt(T* element, std::int32_t argument) {
        Definition::emplaceInt(element, argument);
    }
    static void assignElement(T& destination, T& source) {
        Definition::assignElement(destination, source);
    }
};
} // namespace nijigenerate::compat
