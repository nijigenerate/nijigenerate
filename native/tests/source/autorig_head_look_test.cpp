#include <nijigenerate/autorig/deterministic/detail/head_look_arc.hpp>
#include <nijigenerate/autorig/deterministic/detail/head_contour_curve.hpp>
#include <nijigenerate/autorig/deterministic/workflow_options.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {
using nijigenerate::autorig::deterministic::head_look::Arc;
using Matrix = std::array<double, 9>;
constexpr double radians = std::numbers::pi / 180.;
constexpr double tolerance = 1e-10;

struct Golden {
    double yawDegrees;
    double pitchDegrees;
    double u;
    double v;
    Matrix previewMatrix;
};

// Golden matrices come from proposed(u, v, 1) in the actual preview.
// simulate_head.js SHA256: a011246e3c291e0bade256872818d5d14a63ccbc7fa76ad542ac184c3cb130e1
// The 30-degree group changes only MAX_YAW in that same JS source.
constexpr std::array<Golden, 20> goldens{{
    {45.000000000000000, 20.000000000000000, -1.0000000000000000, -1.0000000000000000,
     {{0.90189385564456792, 0.062542281999422022, -0.42740605530675396,
       0.062542281999422022, 0.96012954067867562, 0.27246968286115808,
       0.42740605530675396, -0.27246968286115808, 0.86202339632324354}}},
    {45.000000000000000, 20.000000000000000, -1.0000000000000000, 0.0000000000000000,
     {{0.70710678118654757, 0.0000000000000000, -0.70710678118654746,
       0.0000000000000000, 1.0000000000000000, 0.0000000000000000,
       0.70710678118654746, 0.0000000000000000, 0.70710678118654757}}},
    {45.000000000000000, 20.000000000000000, -1.0000000000000000, 1.0000000000000000,
     {{0.90189385564456792, -0.062542281999422022, -0.42740605530675396,
       -0.062542281999422022, 0.96012954067867562, -0.27246968286115808,
       0.42740605530675396, 0.27246968286115808, 0.86202339632324354}}},
    {45.000000000000000, 20.000000000000000, 0.0000000000000000, -1.0000000000000000,
     {{1.0000000000000000, 0.0000000000000000, 0.0000000000000000,
       0.0000000000000000, 0.93969262078590843, 0.34202014332566871,
       0.0000000000000000, -0.34202014332566871, 0.93969262078590843}}},
    {45.000000000000000, 20.000000000000000, 0.0000000000000000, 0.0000000000000000,
     {{1.0000000000000000, 0.0000000000000000, 0.0000000000000000,
       0.0000000000000000, 1.0000000000000000, 0.0000000000000000,
       0.0000000000000000, 0.0000000000000000, 1.0000000000000000}}},
    {45.000000000000000, 20.000000000000000, 0.0000000000000000, 1.0000000000000000,
     {{1.0000000000000000, 0.0000000000000000, 0.0000000000000000,
       0.0000000000000000, 0.93969262078590843, -0.34202014332566871,
       0.0000000000000000, 0.34202014332566871, 0.93969262078590843}}},
    {45.000000000000000, 20.000000000000000, 1.0000000000000000, -1.0000000000000000,
     {{0.90189385564456792, -0.062542281999422022, 0.42740605530675396,
       -0.062542281999422022, 0.96012954067867562, 0.27246968286115808,
       -0.42740605530675396, -0.27246968286115808, 0.86202339632324354}}},
    {45.000000000000000, 20.000000000000000, 1.0000000000000000, 0.0000000000000000,
     {{0.70710678118654757, 0.0000000000000000, 0.70710678118654746,
       0.0000000000000000, 1.0000000000000000, 0.0000000000000000,
       -0.70710678118654746, 0.0000000000000000, 0.70710678118654757}}},
    {45.000000000000000, 20.000000000000000, 1.0000000000000000, 1.0000000000000000,
     {{0.90189385564456792, 0.062542281999422022, 0.42740605530675396,
       0.062542281999422022, 0.96012954067867562, -0.27246968286115808,
       -0.42740605530675396, 0.27246968286115808, 0.86202339632324354}}},
    {45.000000000000000, 20.000000000000000, 1.0000000000000000, 0.34999999999999998,
     {{0.77600725325091902, 0.055967044423901574, 0.62823596907564694,
       0.055967044423901574, 0.98601602013008116, -0.15697164707452224,
       -0.62823596907564694, 0.15697164707452224, 0.76202327338100018}}},
    {45.000000000000000, 20.000000000000000, 0.34999999999999998, 1.0000000000000000,
     {{0.98141720848473690, 0.032379704729960226, 0.18913439034619556,
       0.032379704729960226, 0.94357977500106693, -0.32955843629095050,
       -0.18913439034619556, 0.32955843629095050, 0.92499698348580384}}},
    {45.000000000000000, 20.000000000000000, 0.40000000000000002, 0.69999999999999996,
     {{0.97836318942609213, 0.023384972919451057, 0.20556899721884425,
       0.023384972919451057, 0.97472562064656054, -0.22217809859825055,
       -0.20556899721884425, 0.22217809859825055, 0.95308881007265256}}},
    {45.000000000000000, 20.000000000000000, -0.40000000000000002, 0.69999999999999996,
     {{0.97836318942609213, -0.023384972919451057, -0.20556899721884425,
       -0.023384972919451057, 0.97472562064656054, -0.22217809859825055,
       0.20556899721884425, 0.22217809859825055, 0.95308881007265256}}},
    {45.000000000000000, 20.000000000000000, 0.59999999999999998, -0.25000000000000000,
     {{0.92374636360483353, -0.022217725704124214, 0.38235981534594232,
       -0.022217725704124214, 0.99352650760279038, 0.11140669349342941,
       -0.38235981534594232, -0.11140669349342941, 0.91727287120762380}}},
    {45.000000000000000, 20.000000000000000, 0.14999999999999999, -0.84999999999999998,
     {{0.99635876455655625, -0.012504372208719030, 0.084337731573707969,
       -0.012504372208719030, 0.95705871626188943, 0.28962433306407454,
       -0.084337731573707969, -0.28962433306407454, 0.95341748081844568}}},
    {30.000000000000000, 20.000000000000000, 1.0000000000000000, 1.0000000000000000,
     {{0.94606083750699799, 0.044068643500094054, 0.32097795313014760,
       0.044068643500094054, 0.96399563415189582, -0.26224105703750084,
       -0.32097795313014760, 0.26224105703750084, 0.91005647165889381}}},
    {30.000000000000000, 20.000000000000000, 1.0000000000000000, 0.34999999999999998,
     {{0.88776952306570922, 0.033121405528123253, 0.45909502982664152,
       0.033121405528123253, 0.99022522639014809, -0.13548790911524713,
       -0.45909502982664152, 0.13548790911524713, 0.87799474945585732}}},
    {30.000000000000000, 20.000000000000000, 0.34999999999999998, 1.0000000000000000,
     {{0.98943139260186130, 0.024228381666478806, 0.14296329898181859,
       0.024228381666478806, 0.94445678072212724, -0.32774132310378423,
       -0.14296329898181859, 0.32774132310378423, 0.93388817332398855}}},
    {30.000000000000000, 20.000000000000000, 0.40000000000000002, 0.69999999999999996,
     {{0.98783215333794672, 0.017179456488937529, 0.15457199974917818,
       0.017179456488937529, 0.97574478595496239, -0.21823606245634422,
       -0.15457199974917818, 0.21823606245634422, 0.96357693929290911}}},
    {30.000000000000000, 20.000000000000000, -0.59999999999999998, -0.20000000000000001,
     {{0.95843699588257247, 0.011694560916808519, -0.28506448773663962,
       0.011694560916808519, 0.99670950745883158, 0.080208447099643462,
       0.28506448773663962, -0.080208447099643462, 0.95514650334140416}}},
}};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void near(double actual, double expected, const std::string& message,
          double allowed = tolerance) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= allowed,
        message + ": actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected));
}

Matrix product(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t col = 0; col < 3; ++col)
            for (std::size_t k = 0; k < 3; ++k)
                result[row * 3 + col] += a[row * 3 + k] * b[k * 3 + col];
    return result;
}

Matrix previewMatrix(const std::array<double, 3>& editorAngles) {
    // This is the existing depth runtime's Euler order, not a reimplementation
    // of the spherical ellipse or its arc-length lookup.
    const double p = -editorAngles[0], y = -editorAngles[1], r = editorAngles[2];
    const double cp = std::cos(p), sp = std::sin(p);
    const double cy = std::cos(y), sy = std::sin(y);
    const double cr = std::cos(r), sr = std::sin(r);
    const Matrix rx{1., 0., 0., 0., cp, -sp, 0., sp, cp};
    const Matrix ry{cy, 0., sy, 0., 1., 0., -sy, 0., cy};
    const Matrix rz{cr, -sr, 0., sr, cr, 0., 0., 0., 1.};
    auto matrix = product(product(rz, ry), rx);
    // A neutral upright Head uses this basis between the preview and local
    // bone coordinates. Conjugate both sides, preserving the local roll sign.
    constexpr std::array basis{-1., -1., 1.};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t col = 0; col < 3; ++col)
            matrix[row * 3 + col] *= basis[row] * basis[col];
    return matrix;
}

void checkProperRotation(const Matrix& matrix, const std::string& label) {
    for (const double value : matrix) require(std::isfinite(value), label + ": non-finite matrix");
    for (std::size_t a = 0; a < 3; ++a) {
        for (std::size_t b = 0; b < 3; ++b) {
            double dot = 0.;
            for (std::size_t row = 0; row < 3; ++row)
                dot += matrix[row * 3 + a] * matrix[row * 3 + b];
            near(dot, a == b ? 1. : 0., label + ": orthogonality");
        }
    }
    const double determinant =
        matrix[0] * (matrix[4] * matrix[8] - matrix[5] * matrix[7])
        - matrix[1] * (matrix[3] * matrix[8] - matrix[5] * matrix[6])
        + matrix[2] * (matrix[3] * matrix[7] - matrix[4] * matrix[6]);
    near(determinant, 1., label + ": determinant");
}

void checkCardinals(double yawDegrees, double pitchDegrees) {
    const double yaw = yawDegrees * radians, pitch = pitchDegrees * radians;
    const Arc arc(yaw, pitch);
    for (const double amount : {-1., -.5, 0., .5, 1.}) {
        const auto horizontal = arc.euler(amount, 0.);
        near(horizontal[0], 0., "horizontal must not introduce pitch", 1e-13);
        near(horizontal[1], amount * yaw, "horizontal limit", 1e-13);
        near(horizontal[2], 0., "horizontal must not introduce roll", 1e-13);
        const auto vertical = arc.euler(0., amount);
        near(vertical[0], amount * pitch, "vertical limit", 1e-13);
        near(vertical[1], 0., "vertical must not introduce yaw", 1e-13);
        near(vertical[2], 0., "vertical must not introduce roll", 1e-13);
    }
}

void checkDefaultHeadYaw() {
    using nijigenerate::autorig::deterministic::defaultHeadYawDegrees;
    const Arc arc(defaultHeadYawDegrees * radians, 20. * radians);
    for (const double input : {-1., 1.}) {
        const auto angles = arc.euler(input, 0.);
        near(angles[1], input * 45. * radians, "default AutoRig yaw endpoint must be 45 degrees");
        near(angles[0], 0., "default AutoRig yaw endpoint pitch");
        near(angles[2], 0., "default AutoRig yaw endpoint roll");
    }
}

double checkGoldens() {
    const Arc arc45(45. * radians, 20. * radians);
    const Arc arc30(30. * radians, 20. * radians);
    double maximumError = 0.;
    for (std::size_t i = 0; i < goldens.size(); ++i) {
        const auto& golden = goldens[i];
        const auto& arc = golden.yawDegrees == 45. ? arc45 : arc30;
        const auto angles = arc.euler(golden.u, golden.v);
        for (const double value : angles) require(std::isfinite(value), "non-finite Euler result");
        const auto actual = previewMatrix(angles);
        const auto label = "JS golden " + std::to_string(i);
        checkProperRotation(actual, label);
        for (std::size_t element = 0; element < actual.size(); ++element) {
            maximumError = std::max(maximumError, std::abs(actual[element] - golden.previewMatrix[element]));
            near(actual[element], golden.previewMatrix[element], label + " element " + std::to_string(element));
        }
    }
    return maximumError;
}

double angleFromFront(const Matrix& matrix) {
    return std::atan2(std::hypot(matrix[2], matrix[5]), matrix[8]);
}

void checkRadialResponseAndSymmetry() {
    const Arc arc(45. * radians, 20. * radians);
    // Each ray uses the same outer arc table. The angular distance from the
    // neutral forward direction must scale with the input magnitude.
    constexpr std::array<std::array<double, 2>, 3> rays{{{1., .35}, {.4, 1.}, {-1., -.6}}};
    for (const auto& ray : rays) {
        const double outerAngle = angleFromFront(previewMatrix(arc.euler(ray[0], ray[1])));
        for (const double fraction : {.25, .5, .75}) {
            const auto matrix = previewMatrix(arc.euler(fraction * ray[0], fraction * ray[1]));
            near(angleFromFront(matrix), fraction * outerAngle, "radial angular response");
        }
    }

    const auto positive = previewMatrix(arc.euler(.6, .4));
    for (const double signU : {-1., 1.}) {
        for (const double signV : {-1., 1.}) {
            const auto reflected = previewMatrix(arc.euler(signU * .6, signV * .4));
            const std::array mirror{signU, signV, 1.};
            for (std::size_t row = 0; row < 3; ++row)
                for (std::size_t col = 0; col < 3; ++col)
                    near(reflected[row * 3 + col], mirror[row] * positive[row * 3 + col] * mirror[col],
                        "quadrant reflection");
        }
    }

    for (const auto& input : std::array<std::array<double, 2>, 4>{
             std::array{1e-10, .8}, std::array{.8, 1e-10},
             std::array{-1e-10, -.8}, std::array{-.8, -1e-10}}) {
        checkProperRotation(previewMatrix(arc.euler(input[0], input[1])), "near-axis finite rotation");
    }
}
void checkContourJoins() {
    using Point = std::array<double, 2>;
    nijigenerate::autorig::deterministic::head_surface::ContourCurve<Point> curve;
    const std::array<Point, 5> points{{{0.,0.}, {1.,2.}, {.7,3.}, {1.1,3.2}, {2.,4.}}};
    std::array<double, points.size()> lengths{};
    for (std::size_t i=0; i<points.size(); ++i) {
        curve.append(points[i]);
        lengths[i]=curve.length();
        curve.append(points[i]); // The adjoining guides can share a knot.
    }
    curve.fit();
    for (std::size_t i=0; i<points.size(); ++i) {
        const auto value=curve.at(lengths[i]);
        for (std::size_t axis=0; axis<2; ++axis)
            near(value[axis], points[i][axis], "contour retains anchors");
    }
    for (std::size_t i=1; i+1<points.size(); ++i) {
        constexpr double h=1e-6;
        const auto before=curve.at(lengths[i]-h), center=curve.at(lengths[i]), after=curve.at(lengths[i]+h);
        for (std::size_t axis=0; axis<2; ++axis)
            if (std::abs((after[axis]-2.*center[axis]+before[axis])/h)>1e-5)
                throw std::runtime_error("Contour tangent jumps at a join");
    }
    for (std::size_t i=0; i+1<points.size(); ++i) {
        for (int step=1; step<32; ++step) {
            const auto p=curve.at(std::lerp(lengths[i],lengths[i+1],step/32.));
            for (std::size_t axis=0; axis<2; ++axis) {
                const auto [lo,hi]=std::minmax(points[i][axis],points[i+1][axis]);
                if (p[axis]<lo-tolerance || p[axis]>hi+tolerance)
                    throw std::runtime_error("Contour overshoots between knots");
            }
        }
    }
}
} // namespace

int main() {
    try {
        checkDefaultHeadYaw();
        checkCardinals(45., 20.);
        checkCardinals(30., 20.);
        const double maximumError = checkGoldens();
        checkRadialResponseAndSymmetry();
        checkContourJoins();
        std::cout << "PASS: default AutoRig yaw +/-45 degrees; head-look arc; "
            "20 independent JS matrix goldens, 45/20 and 30/20 cardinals, "
            "radial response, quadrant symmetry, finite near-axis rotations, continuous contour joins.\n"
            << std::setprecision(17) << "Maximum golden matrix error: " << maximumError << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

