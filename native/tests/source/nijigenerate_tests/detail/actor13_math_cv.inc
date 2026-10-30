// Existing regression.d20276-20357 and 20398-20453 only.
void testCoreMathPathExtraction() {
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::compat::SourceMirAbi64;
    using nijigenerate::compat::ngSourceMutableByteArraySlice;
    using nijigenerate::core::math::path::extractPath;
    using nijigenerate::core::tasks::TaskFiber;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    const auto runExtract = []<class T>(T skeleton, std::int32_t width, std::int32_t height) {
        Slice<Vec2u> result;
        auto fiber = TaskFiber::createSourceDeferred([&] {
            return [&] { result = extractPath(skeleton, width, height); };
        });
        while (fiber->state() != TaskFiber::State::term) fiber->call();
        return result;
    };
    Slice<std::uint8_t> emptyData;
    emptyData.resize(9);
    std::int32_t error = 0;
    auto empty = ngSourceMutableByteArraySlice<SourceMirAbi64>(emptyData).reshape({3, 3}, error);
    check(runExtract(empty, 3, 3).size() == 0, "extractPath should return empty path for empty skeleton");
    Slice<std::uint8_t> data;
    data.resize(25);
    data[2 * 5 + 1] = 1;
    data[2 * 5 + 2] = 1;
    data[2 * 5 + 3] = 1;
    auto line = ngSourceMutableByteArraySlice<SourceMirAbi64>(data).reshape({5, 5}, error);
    auto path = runExtract(line, 5, 5);
    check(path.size() == 3, "extractPath should keep the longest connected line");
    check(path[0] == Vec2u{1, 2} || path[0] == Vec2u{3, 2}, "extractPath should start at one endpoint");
    check(path[path.size() - 1] == Vec2u{1, 2} || path[path.size() - 1] == Vec2u{3, 2},
        "extractPath should end at the other endpoint");
    Slice<std::uint8_t> branches;
    branches.resize(36);
    for (std::int32_t x = 0; x < 5; ++x) branches[1 * 6 + x] = 1;
    branches[4 * 6 + 4] = 1;
    branches[5 * 6 + 4] = 1;
    auto branch = ngSourceMutableByteArraySlice<SourceMirAbi64>(branches).reshape({6, 6}, error);
    auto longest = runExtract(branch, 6, 6);
    check(longest.size() == 5, "extractPath should select the longest component among disconnected components");
}

void testCoreMathSkeletonizeInvariants() {
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::compat::SourceMirAbi64;
    using nijigenerate::compat::ngSourceMutableByteArraySlice;
    using nijigenerate::core::math::path::extractPath;
    using nijigenerate::core::math::skeletonize::skeletonizeImage;
    using nijigenerate::core::tasks::TaskFiber;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    std::int32_t error = 0;
    Slice<std::uint8_t> data;
    data.resize(49);
    for (std::int32_t y = 1; y < 6; ++y)
        for (std::int32_t x = 1; x < 6; ++x) data[y * 7 + x] = 1;
    auto image = ngSourceMutableByteArraySlice<SourceMirAbi64>(data).reshape({7, 7}, error);
    std::size_t before = 0;
    for (auto value : data) before += value ? 1 : 0;
    auto fiber = TaskFiber::createSourceDeferred([&] { return [&] { skeletonizeImage(image); }; });
    while (fiber->state() != TaskFiber::State::term) fiber->call();
    std::size_t after = 0;
    for (auto value : data) after += value ? 1 : 0;
    check(after > 0, "skeletonizeImage should preserve a skeleton for a filled square");
    check(after < before, "skeletonizeImage should thin a filled square");
    check(data[3 * 7 + 3] != 0, "skeletonizeImage should keep the center of a symmetric filled square");
    Slice<Vec2u> path;
    auto extractFiber = TaskFiber::createSourceDeferred([&] { return [&] { path = extractPath(image, 7, 7); }; });
    while (extractFiber->state() != TaskFiber::State::term) extractFiber->call();
    check(path.size() > 0, "skeletonized bitmap should produce an extractable path");
    for (auto point : path)
        check(point.x < 7 && point.y < 7 && image(point.y, point.x) != 0,
            "extracted skeleton path should stay on foreground pixels");
}

void testCoreCvImageContoursDistanceTransform() {
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::SourceMirAbi64;
    using nijigenerate::compat::ngSourceMutableByteArraySlice;
    using nijigenerate::core::tasks::TaskFiber;
    using namespace nijigenerate::core::cv::image;
    using namespace nijigenerate::core::cv::distancetransform;
    using namespace nijigenerate::core::cv::contours;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, nijigenerate::compat::RetainedString::fromStaticLiteral(message));
    };
    const auto shapeEquals = [](auto shape, Slice<std::uint64_t> expected) {
        if (shape.size() != expected.size()) return false;
        for (std::size_t i = 0; i < shape.size(); ++i) if (shape[i] != expected[i]) return false;
        return true;
    };
    auto mono = ngCreateImage(3, 2, ImageFormat::IF_MONO);
    check(mono->width() == 3 && mono->height() == 2 && mono->channels() == 1,
        "mono image should expose dimensions and channel count");
    auto monoShape = mono->shape();
    check(shapeEquals(monoShape, Slice<std::uint64_t>{2, 3, 1}), "mono image shape should be height,width,channels");
    check(mono->data().size() == 6, "allocated mono image should allocate width*height bytes");
    auto rgba = ngCreateImageDeferred([] { return 2; }, [] { return 2; }, [] { return ImageFormat::IF_RGB_ALPHA; },
        [] { return BitDepth::BD_8; }, [] { Slice<std::uint8_t> bytes; bytes.resize(16); return bytes; });
    const bool rgbaShape = [&] {
        if (rgba->channels() != 4) return false;
        auto shape = rgba->sliced().shape();
        return shapeEquals(shape, Slice<std::uint64_t>{2, 2, 4});
    }();
    check(rgbaShape, "rgba image should expose 4-channel slice shape");
    auto input = Slice<float>{0.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), 0.0f};
    auto dt = dt1d(std::span<const float>(input.data(), input.size()));
    check(std::get<0>(dt).size() == 4 && std::get<0>(dt)[0] == 0 && std::get<0>(dt)[3] == 0,
        "dt1d should preserve zero sites");
    check(std::get<0>(dt)[1] > 0 && std::get<0>(dt)[2] > 0, "dt1d should assign positive distance to non-site entries");
    Slice<std::uint8_t> bytes{0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0,
        0, 1, 1, 1, 0, 0, 0, 0, 0, 0};
    std::int32_t error = 0;
    auto binary = ngSourceMutableByteArraySlice<SourceMirAbi64>(bytes).reshape({5, 5}, error);
    DistanceView distance;
    NearestView nearest;
    distanceTransform(binary, distance, nearest);
    auto distanceShape = distance.shape();
    check(shapeEquals(distanceShape, Slice<std::uint64_t>{5, 5}), "distance transform should produce 2D distance slice");
    auto nearestShape = nearest.shape();
    check(shapeEquals(nearestShape, Slice<std::uint64_t>{5, 5, 2}), "distance transform should produce nearest coordinate slice");
    check(distance.at(0, 0) == 0 && distance.at(2, 2) == 0, "background pixels should have zero distance");
    check(distance.at(1, 1) > 0, "foreground pixels should have positive distance");
    Contours contours;
    Slice<ContourHierarchy> hierarchy;
    auto fiber = TaskFiber::createSourceDeferred([&] {
        return [&] { findContours(binary, contours, hierarchy, RetrievalMode::TREE, ApproximationMethod::SIMPLE); };
    });
    while (fiber->state() != TaskFiber::State::term) fiber->call();
    check(contours.size() > 0, "findContours should find contours in a ring image");
    check(hierarchy.size() == contours.size(), "findContours should return matching hierarchy entries");
    for (auto entry : hierarchy)
        check(entry.parent >= -1 && entry.parent < static_cast<std::int32_t>(hierarchy.size()), "hierarchy parent should be valid");
    Contour polygon{Vec2i{0, 0}, Vec2i{4, 0}, Vec2i{4, 4}, Vec2i{0, 4}, Vec2i{0, 0}};
    check(pointInPolygon(Vec2i{2, 2}, polygon), "pointInPolygon should detect inside points");
    check(!pointInPolygon(Vec2i{5, 5}, polygon), "pointInPolygon should reject outside points");
    check(inBounds(4, 4, 5, 5) && !inBounds(5, 4, 5, 5), "inBounds should respect image dimensions");
    check(distanceToSegment(Vec2i{2, 2}, Vec2i{0, 0}, Vec2i{4, 0}) > 1.9,
        "distanceToSegment should measure perpendicular distance");
    auto simplified = approximateContour(Contour{Vec2i{0, 0}, Vec2i{1, 0}, Vec2i{2, 0}, Vec2i{2, 1}}, ApproximationMethod::SIMPLE);
    check(simplified.size() == 3, "SIMPLE contour approximation should remove collinear middle points");
}
