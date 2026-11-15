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
#include <vector>

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

double ngRigRepairGridOrientation(compat::Slice<contracts::Point2> grid,
    std::size_t columns, compat::Slice<contracts::Point2> rest) {
    ngRigValidateGrid(rest, columns, 0);
    if (grid.size() != rest.size())
        throw compat::ThrowableException{contracts::detail::copyContractMessage("Orientation correction grid dimensions differ"),
            compat::RetainedString::fromStaticLiteral("native/autorig/geometry_json.cpp"), 0};
    try { ngRigValidateGrid(grid, columns, .08, rest); return 0; }
    catch (const compat::ThrowableException&) {}
    const auto original = grid.duplicate();
    std::vector<std::array<std::size_t, 3>> triangles;
    for (std::size_t y = 0; y + 1 < rest.size() / columns; ++y)
        for (std::size_t x = 0; x + 1 < columns; ++x) {
            const auto a = y * columns + x, b = a + 1, c = a + columns, d = c + 1;
            for (const auto triangle : {std::array{a,b,c}, std::array{a,b,d},
                    std::array{a,d,c}, std::array{b,d,c}}) triangles.push_back(triangle);
        }
    // Reserve area for interpolation between independently projected angle keys.
    // A key-only 3% margin can collapse below the 2% verification limit in between.
    // Project only violated area constraints. This keeps the authored motion and
    // corrects local silhouette compression rather than reducing the control angle.
    for (unsigned iteration = 0; iteration < 500; ++iteration) {
        double minimum = 1;
        for (const auto indices : triangles) {
            const auto a = grid[indices[0]], b = grid[indices[1]], c = grid[indices[2]];
            const double reference = detail::cross(detail::sub(rest[indices[1]], rest[indices[0]]),
                detail::sub(rest[indices[2]], rest[indices[0]]));
            const double area = detail::cross(detail::sub(b,a), detail::sub(c,a));
            minimum = std::min(minimum, area / reference);
            if (area >= .1 * reference) continue;
            const std::array<contracts::Point2,3> gradient{{
                {b[1]-c[1],c[0]-b[0]}, {c[1]-a[1],a[0]-c[0]}, {a[1]-b[1],b[0]-a[0]}}};
            double denominator = 0;
            for (const auto g : gradient) denominator += g[0]*g[0] + g[1]*g[1];
            if (!(denominator > 0) || !std::isfinite(denominator)) break;
            const double step = (.1 * reference - area) / denominator;
            for (std::size_t i = 0; i < 3; ++i) for (std::size_t axis = 0; axis < 2; ++axis)
                grid[indices[i]][axis] += step * gradient[i][axis];
        }
        if (minimum > .099) break;
    }
    ngRigValidateGrid(grid, columns, .095, rest);
    double maximum = 0;
    for (std::size_t i = 0; i < grid.size(); ++i)
        maximum = std::max(maximum, std::hypot(grid[i][0]-original[i][0], grid[i][1]-original[i][1]));
    const auto span = detail::sub(rest[rest.size()-1], rest[0]);
    if (maximum > .1 * std::min(span[0],span[1]))
        throw compat::ThrowableException{contracts::detail::copyContractMessage("Projected grid requires excessive orientation correction"),
            compat::RetainedString::fromStaticLiteral("native/autorig/geometry_json.cpp"), 0};
    return maximum;
}

double ngRigRepairGridInterpolation(compat::Slice<compat::Slice<contracts::Point2>> keys,
    std::size_t nx, std::size_t ny, std::size_t columns, compat::Slice<contracts::Point2> rest) {
    if (nx < 2 || ny < 2) return 0;
    if (keys.size() != nx * ny) throw std::runtime_error("Interpolation key dimensions differ");
    std::vector<compat::Slice<contracts::Point2>> original;
    std::vector<bool> locked;
    for (auto key : keys) {
        if (key.size() != rest.size()) throw std::runtime_error("Interpolation grid dimensions differ");
        original.push_back(key.duplicate());
        bool neutral = true;
        for (std::size_t i = 0; i < rest.size(); ++i)
            if (std::hypot(key[i][0]-rest[i][0], key[i][1]-rest[i][1]) > .001) neutral = false;
        locked.push_back(neutral);
    }
    struct Triangle { std::array<std::size_t,3> vertices; double area; };
    std::vector<Triangle> triangles;
    for (std::size_t y = 0; y + 1 < rest.size()/columns; ++y)
        for (std::size_t x = 0; x + 1 < columns; ++x) {
            const auto a=y*columns+x,b=a+1,c=a+columns,d=c+1;
            for (auto ids : {std::array{a,b,c},std::array{a,b,d},std::array{a,d,c},std::array{b,d,c}})
                triangles.push_back({ids,detail::cross(detail::sub(rest[ids[1]],rest[ids[0]]),
                    detail::sub(rest[ids[2]],rest[ids[0]]))});
        }
    // Project area constraints in the interpolated pose back to its four keys.
    // Keep neutral exact and leave every already-safe vertex unchanged.
    bool converged=false;
    for (unsigned iteration=0; iteration<100; ++iteration) {
        double minimum=1;
        for (std::size_t x=0;x+1<nx;++x) for (std::size_t y=0;y+1<ny;++y)
            for (unsigned u=0;u<=8;++u) for (unsigned v=0;v<=8;++v) {
                const double fx=u/8.,fy=v/8.;
                const std::array<std::size_t,4> ids{x*ny+y,(x+1)*ny+y,x*ny+y+1,(x+1)*ny+y+1};
                const std::array<double,4> weights{(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
                double weightNorm=0;
                for(std::size_t k=0;k<4;++k)if(!locked[ids[k]])weightNorm+=weights[k]*weights[k];
                for (const auto& triangle : triangles) {
                    std::array<contracts::Point2,3> p{contracts::Point2{0,0},contracts::Point2{0,0},contracts::Point2{0,0}};
                    for(std::size_t t=0;t<3;++t)for(std::size_t k=0;k<4;++k)for(std::size_t a=0;a<2;++a)
                        p[t][a]+=weights[k]*keys[ids[k]][triangle.vertices[t]][a];
                    const double area=detail::cross(detail::sub(p[1],p[0]),detail::sub(p[2],p[0]));
                    if(!std::isfinite(area)||!(triangle.area>0))throw std::runtime_error("Invalid interpolation grid area");
                    minimum=std::min(minimum,area/triangle.area);
                    if(area>=.04*triangle.area)continue;
                    const std::array<contracts::Point2,3> gradient{{
                        {p[1][1]-p[2][1],p[2][0]-p[1][0]},
                        {p[2][1]-p[0][1],p[0][0]-p[2][0]},
                        {p[0][1]-p[1][1],p[1][0]-p[0][0]}}};
                    double norm=0;for(auto g:gradient)norm+=g[0]*g[0]+g[1]*g[1];
                    if(norm*weightNorm<=0)throw std::runtime_error("Locked interpolation constraint is degenerate");
                    const double step=(.04*triangle.area-area)/(norm*weightNorm);
                    for(std::size_t k=0;k<4;++k)if(!locked[ids[k]])
                        for(std::size_t t=0;t<3;++t)for(std::size_t a=0;a<2;++a)
                            keys[ids[k]][triangle.vertices[t]][a]+=step*weights[k]*gradient[t][a];
                }
            }
        if(minimum>.035){converged=true;break;}
    }
    if(!converged)throw std::runtime_error("Interpolation orientation correction did not converge");
    double maximum=0;
    for(std::size_t k=0;k<keys.size();++k)for(std::size_t i=0;i<rest.size();++i)
        maximum=std::max(maximum,std::hypot(keys[k][i][0]-original[k][i][0],keys[k][i][1]-original[k][i][1]));
    const auto span=detail::sub(rest[rest.size()-1],rest[0]);
    if(maximum>.1*std::min(span[0],span[1]))throw std::runtime_error("Excessive interpolation orientation correction");
    return maximum;
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
