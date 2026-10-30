#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/math/welding.hpp>
#include <nodes/drawable.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <resource.hpp>
#include <new>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::operations::impl {
void incUpdateWeldedPoints(std::shared_ptr<nicxlive::core::nodes::Drawable> drawable,
    compat::ConstSlice<std::ptrdiff_t> remap) {
    const auto links = [&] {
        try {
            return compat::ngRequireObject(drawable)->weldedLinks.duplicate();
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
    }();
    for (auto welded : links) {
        auto target = welded.target.lock();
        std::ptrdiff_t counter = -1;
        const auto counterLinks=compat::ngRequireObject(target)->weldedLinks;
        for (std::size_t i = 0; i < counterLinks.size(); ++i)
            if (nicxlive::core::ngResourceObjectEquals(counterLinks[i].target.lock(), drawable)) {
                counter = static_cast<std::ptrdiff_t>(i);
                break;
            }
        if (counter < 0) continue;
        const auto reverse = [&] {
            const auto currentLinks = compat::ngRequireObject(target)->weldedLinks;
            const auto index = static_cast<std::size_t>(counter);
            if (index >= currentLinks.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                index, currentLinks.size(), compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/operations/impl.d"), 291);
            return currentLinks[index].indices;
        }();
        const auto updated = core::math::welding::ngRemapWeldingLinks(
            {welded.indices.data(), welded.indices.size()}, {reverse.data(), reverse.size()},
            remap.view(), drawable->getVertices().size());
        core::actionstack::incActionPush(actions::node::DrawableChangeWeldingAction::create(
            drawable, target, updated.forward, welded.weight, updated.reverse));
    }
}
}
