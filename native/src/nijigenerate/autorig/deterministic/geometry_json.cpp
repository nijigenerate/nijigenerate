#include "geometry.hpp"
#include "contracts_json.hpp"
#include "linear.hpp"
#include "detail/geometry_scalar.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <string_view>

namespace nijigenerate::autorig::deterministic::geometry {
namespace {
core::settings::SettingsJsonValue meshMember(core::settings::SettingsJsonValue& object,
    std::string_view key) {
    return object.sourceIndexValue(key);
}
}

bool ngRigSameTextureMapping(core::settings::SettingsJsonValue a, core::settings::SettingsJsonValue b) {
    const compat::Slice<compat::RetainedString> keys{
        compat::RetainedString::fromStaticLiteral("vertices"),
        compat::RetainedString::fromStaticLiteral("uv"),
        compat::RetainedString::fromStaticLiteral("triangles")};
    for (const auto keyDescriptor : keys) {
        const auto key = keyDescriptor.view();
        const auto actualCount = meshMember(a, key).sourceArrayValue().size();
        const auto expectedCount = meshMember(b, key).sourceArrayValue().size();
        if (actualCount != expectedCount) return false;
        // Source foreach captures the range descriptor, but repeated b lookups remain live.
        const auto rows = meshMember(a, key).sourceArrayValue();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto actual = contracts::ngRigNumbers(rows[i]);
            const auto expected = contracts::ngRigNumbers(meshMember(b, key).sourceIndexValue(i));
            if (actual.size() != expected.size()) return false;
            const double tolerance = std::string_view(key) == "triangles" ? 0 :
                std::string_view(key) == "uv" ? 1e-7 : 1e-5;
            for (std::size_t j = 0; j < actual.size(); ++j)
                if (std::abs(actual[j] - expected[j]) > tolerance) return false;
        }
    }
    return true;
}

double ngRigTriangleMinimumRatio(compat::Slice<contracts::Point2> rest,
    core::settings::SettingsJsonValue triangles, compat::Slice<double> offsets) {
    const auto enforce = []<std::size_t N>(bool condition, const char (&message)[N], std::size_t sourceLine) {
        if (!condition) throw compat::ThrowableException{contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"),
            sourceLine};
    };
    enforce(offsets.size() == rest.size() * 2, "Triangle validation offset count mismatch", 51);
    double minimum = std::numeric_limits<double>::infinity();
    const auto triangleRows = triangles.sourceArrayValue();
    for (const auto triangle : triangleRows) {
        const auto values = contracts::ngRigNumbers(triangle);
        enforce(values.size() == 3, "Invalid validation triangle", 54);
        std::size_t indices[3];
        contracts::Point2 base[3], posed[3];
        for (std::size_t i = 0; i < 3; ++i) {
            enforce(values[i] >= 0 && values[i] < static_cast<double>(rest.size()) &&
                values[i] == static_cast<std::size_t>(values[i]), "Invalid triangle index", 58);
            indices[i] = static_cast<std::size_t>(values[i]);
            base[i] = rest[indices[i]];
            posed[i] = contracts::Point2{base[i][0] + offsets[indices[i] * 2],
                base[i][1] + offsets[indices[i] * 2 + 1]};
            enforce(std::isfinite(posed[i][0]) && std::isfinite(posed[i][1]), "Nonfinite triangle pose", 61);
        }
        const double area = detail::cross(detail::sub(base[1], base[0]), detail::sub(base[2], base[0]));
        if (area == 0) continue;
        const double ratio = detail::cross(detail::sub(posed[1], posed[0]), detail::sub(posed[2], posed[0])) / area;
        minimum = ratio < minimum ? ratio : minimum;
    }
    return minimum;
}

double ngRigVerifySourceUV(core::settings::SettingsJsonValue sourceRoot,
    core::settings::SettingsJsonValue currentRoot, double tolerance) {
    const auto enforce = []<std::size_t N>(bool condition, const char (&message)[N], std::size_t sourceLine) {
        if (!condition) throw compat::ThrowableException{contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"),
            sourceLine};
    };
    const auto original = contracts::ngRigPoints(meshMember(sourceRoot, "vertices"));
    const auto uv = contracts::ngRigPoints(meshMember(sourceRoot, "uv"));
    enforce(original.size() == uv.size() && uv.size() >= 3, "Invalid source UV frame", 73);
    linear::RigMatrix design, values;
    for (std::size_t i = 0; i < uv.size(); ++i) {
        const auto point = uv[i];
        design.append(linear::RigRow{point[0], point[1], 1.});
        values.append(linear::RigRow{original[i][0], original[i][1]});
    }
    const auto fitted = linear::ngRigLeastSquares(design, values);
    double maximum = 0;
    const auto check = [&](contracts::Point2 point, contracts::Point2 texture) {
        for (std::size_t axis = 0; axis < 2; ++axis) {
            const double expected = texture[0] * fitted[0][axis] + texture[1] * fitted[1][axis] + fitted[2][axis];
            enforce(std::isfinite(point[axis]) && std::isfinite(expected), "Nonfinite source UV frame", 81);
            const double difference = std::abs(point[axis] - expected);
            maximum = maximum < difference ? difference : maximum;
        }
    };
    for (std::size_t i = 0; i < original.size(); ++i) check(original[i], uv[i]);
    const auto current = contracts::ngRigPoints(meshMember(currentRoot, "vertices"));
    const auto texture = contracts::ngRigPoints(meshMember(currentRoot, "uv"));
    enforce(current.size() == texture.size() && current.size() >= 3, "Invalid remeshed UV frame", 87);
    for (std::size_t i = 0; i < current.size(); ++i) check(current[i], texture[i]);
    if (!(maximum <= tolerance)) {
        // std.conv.to!string(double) uses default FormatSpec ('s', unspecified precision).
        // enforce takes a lazy message, so successful validation never formats it.
        const std::array<core::translation::TypedFormatArgument, 1> arguments{
            core::translation::ngFormatArgument(maximum)};
        const auto formatted = core::translation::ngFormatTyped("%s", arguments);
        const auto message = contracts::detail::allocateContractStorage([&] {
            return compat::RetainedString(
                std::string("Remeshing or hierarchy changed the source UV-to-root frame: ") + formatted);
        });
        const auto copied = contracts::detail::copyContractMessage(message.view());
        throw compat::ThrowableException{copied,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"), 90};
    }
    return maximum;
}

core::settings::SettingsJsonValue ngRigSourceUVRegistration(core::settings::SettingsJsonValue source,
    core::settings::SettingsJsonValue current) {
    const auto enforce = []<std::size_t N>(bool condition, const char (&message)[N], std::size_t sourceLine) {
        if (!condition) throw compat::ThrowableException{contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"),
            sourceLine};
    };
    const auto affine = [&](core::settings::SettingsJsonValue mesh) {
        const auto vertices = contracts::ngRigPoints(meshMember(mesh, "vertices"));
        const auto uv = contracts::ngRigPoints(meshMember(mesh, "uv"));
        enforce(vertices.size() == uv.size() && vertices.size() >= 3, "Invalid UV registration mesh", 98);
        linear::RigMatrix design, values;
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto point = vertices[i];
            design.append(linear::RigRow{point[0], point[1], 1.});
            values.append(linear::RigRow{uv[i][0], uv[i][1]});
        }
        const auto fitted = linear::ngRigLeastSquares(design, values);
        for (std::size_t i = 0; i < vertices.size(); ++i) for (std::size_t axis = 0; axis < 2; ++axis) {
            const auto point = vertices[i];
            enforce(std::abs(point[0] * fitted[0][axis] + point[1] * fitted[1][axis] +
                fitted[2][axis] - uv[i][axis]) <= 1e-6, "Source UVs require non-affine registration", 103);
        }
        return fitted;
    };
    const auto a = affine(source), b = affine(current);
    const double determinant = a[0][0] * a[1][1] - a[0][1] * a[1][0];
    enforce(std::abs(determinant) > 1e-20, "Singular source UV mapping", 109);
    const double inverse[2][2]{{a[1][1] / determinant, -a[0][1] / determinant},
        {-a[1][0] / determinant, a[0][0] / determinant}};
    double transform[2][2];
    for (std::size_t i = 0; i < 2; ++i) for (std::size_t j = 0; j < 2; ++j)
        transform[i][j] = b[i][0] * inverse[0][j] + b[i][1] * inverse[1][j];
    enforce(std::abs(transform[0][1]) <= 1e-6 && std::abs(transform[1][0]) <= 1e-6 &&
        transform[0][0] > 0 && transform[1][1] > 0, "UV registration needs an unsupported sheared Part frame", 115);
    const contracts::Point2 shift{(b[2][0] - a[2][0]) * inverse[0][0] + (b[2][1] - a[2][1]) * inverse[1][0],
        (b[2][0] - a[2][0]) * inverse[0][1] + (b[2][1] - a[2][1]) * inverse[1][1]};
    using core::settings::SettingsJsonValue;
    core::settings::SettingsJsonArray scaleJson{SettingsJsonValue(transform[0][0]), SettingsJsonValue(transform[1][1])};
    core::settings::SettingsJsonArray shiftJson{SettingsJsonValue(shift[0]), SettingsJsonValue(shift[1])};
    core::settings::SettingsJsonArray affineJson;
    for (const auto row : a) {
        core::settings::SettingsJsonArray entries;
        for (const auto value : row) entries.append(SettingsJsonValue(value));
        affineJson.append(SettingsJsonValue(entries));
    }
    core::settings::SettingsJsonObject result;
    result[compat::RetainedString::fromStaticLiteral("scale")] = SettingsJsonValue(scaleJson);
    result[compat::RetainedString::fromStaticLiteral("shift")] = SettingsJsonValue(shiftJson);
    result[compat::RetainedString::fromStaticLiteral("source_affine")] = SettingsJsonValue(affineJson);
    return SettingsJsonValue(result);
}

core::settings::SettingsJsonValue ngRigValidateGrid(compat::Slice<contracts::Point2> grid,
    std::size_t columns, double minimumRatio, compat::Slice<contracts::Point2> rest) {
    const auto enforce = []<std::size_t N>(bool condition, const char (&message)[N], std::size_t sourceLine) {
        if (!condition) throw compat::ThrowableException{contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/geometry.d"),
            sourceLine};
    };
    const auto minimum = [](double a, double b) { return b < a ? b : a; };
    const auto maximum = [](double a, double b) { return a < b ? b : a; };
    enforce(columns >= 2 && grid.size() % columns == 0 && grid.size() / columns >= 2,
        "Invalid grid dimensions", 137);
    enforce(std::isfinite(minimumRatio) && minimumRatio >= 0, "Invalid minimum area ratio", 139);
    enforce(rest.isNull() || rest.size() == grid.size(), "Rest grid dimensions differ from posed grid", 140);
    auto low = grid[0], high = grid[0];
    for (const auto point : grid) for (std::size_t axis = 0; axis < 2; ++axis) {
        enforce(std::isfinite(point[axis]), "Nonfinite grid point", 143);
        low[axis] = minimum(low[axis], point[axis]);
        high[axis] = maximum(high[axis], point[axis]);
    }
    const std::size_t rows = grid.size() / columns, cells = (rows - 1) * (columns - 1);
    const double reference = (high[0] - low[0]) * (high[1] - low[1]) / cells;
    double smallest = std::numeric_limits<double>::infinity(), smallestRatio = smallest;
    for (std::size_t y = 0; y < rows - 1; ++y) for (std::size_t x = 0; x < columns - 1; ++x) {
        const auto p00 = grid[y * columns + x], p10 = grid[y * columns + x + 1];
        const auto p01 = grid[(y + 1) * columns + x], p11 = grid[(y + 1) * columns + x + 1];
        const double corners[]{detail::cross(detail::sub(p10, p00), detail::sub(p01, p00)),
            detail::cross(detail::sub(p10, p00), detail::sub(p11, p10)),
            detail::cross(detail::sub(p11, p01), detail::sub(p01, p00)),
            detail::cross(detail::sub(p11, p01), detail::sub(p11, p10))};
        double cellReference = reference;
        if (!rest.isNull()) {
            const auto origin = rest[y * columns + x];
            cellReference = detail::cross(detail::sub(rest[y * columns + x + 1], origin),
                detail::sub(rest[(y + 1) * columns + x], origin));
            enforce(cellReference > 0, "Folded or degenerate rest grid", 158);
        }
        const auto threshold = maximum(std::numeric_limits<double>::min(), cellReference * minimumRatio);
        for (const auto value : corners) {
            enforce(value > threshold, "Folded or degenerate rig grid", 162);
            smallest = minimum(smallest, value);
            smallestRatio = minimum(smallestRatio, value / cellReference);
        }
    }
    core::settings::SettingsJsonObject result;
    result[compat::RetainedString::fromStaticLiteral("cells")] = core::settings::SettingsJsonValue(cells);
    result[compat::RetainedString::fromStaticLiteral("min_jacobian")] = core::settings::SettingsJsonValue(smallest);
    result[compat::RetainedString::fromStaticLiteral("reference_cell_area")] = core::settings::SettingsJsonValue(reference);
    result[compat::RetainedString::fromStaticLiteral("min_area_ratio")] = core::settings::SettingsJsonValue(smallestRatio);
    result[compat::RetainedString::fromStaticLiteral("local_orientation_preserved")] = core::settings::SettingsJsonValue(true);
    result[compat::RetainedString::fromStaticLiteral("global_injectivity_tested")] = core::settings::SettingsJsonValue(false);
    return core::settings::SettingsJsonValue(result);
}
} // namespace nijigenerate::autorig::deterministic::geometry
