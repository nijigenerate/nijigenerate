// Complete existing regression.d1267-1302 only.
void testOnionSliceCaptureGeometry() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec4;
    using nicxlive::core::math::Mat4;
    using nicxlive::core::math::Vec2Array;
    using nijigenerate::viewport::model::onionslice::OnionSliceGeometry;
    using nijigenerate::ext::nodes::excamera_detail::point;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto xy = [](Vec4 value) { return Vec2{value.x, value.y}; };
    auto worldMin = Vec2{-850, -2200};
    auto worldMax = Vec2{1200, 2600};
    auto projection = Mat4::orthographic(-900.0f, 900.0f, 650.0f, -650.0f, -100.0f, 100.0f);
    for (auto zoom : std::array{0.24f, 1.0f, 2.0f}) {
        for (auto mirror : std::array{1.0f, -1.0f}) {
            auto captureCamera = projection * Mat4::scale(Vec3{zoom * mirror, zoom, 1}) *
                Mat4::translation(-133.91f, 172.45f, 0);
            auto capturedRender = captureCamera * Mat4::scale(Vec3{1 / zoom, 1 / zoom, 1});
            auto area = nicxlive::core::Rect{worldMin.x * zoom, worldMin.y * zoom,
                (worldMax.x - worldMin.x) * zoom, (worldMax.y - worldMin.y) * zoom};
            auto vertices = nicxlive::core::math::ngVecaFromVectorsSource(
                nicxlive::common::SharedArray<Vec2>{Vec2{area.x, area.y}, Vec2{area.x, area.y + area.h},
                    Vec2{area.x + area.w, area.y}, Vec2{area.x + area.w, area.y + area.h}});
            auto originalVertices = vertices.dup();
            auto geometry = OnionSliceGeometry(vertices, captureCamera, capturedRender, Mat4::identity());
            check(nearVec2Array(vertices, originalVertices),
                "capturing onion bounds must not mutate the shared snapshot vertices");
            check(near(geometry.area.x, area.x) && near(geometry.area.y, area.y) &&
                near(geometry.area.w, area.w) && near(geometry.area.h, area.h),
                "onion bounds must include all four snapshot vertices");
            for (auto view : std::array{captureCamera, projection * Mat4::scale(Vec3{-0.8f, 0.8f, 1}) *
                    Mat4::zRotation(0.3f) * Mat4::translation(300.0f, -200.0f, 0)}) {
                auto actualMin = point(geometry.matrix(view), Vec4{area.x, area.y, 0, 1});
                auto actualMax = point(geometry.matrix(view), Vec4{area.x + area.w, area.y + area.h, 0, 1});
                check(nearVec2(xy(actualMin), xy(point(view, Vec4{worldMin.x, worldMin.y, 0, 1}))) &&
                    nearVec2(xy(actualMax), xy(point(view, Vec4{worldMax.x, worldMax.y, 0, 1}))),
                    "onion images must cover the captured model extent after zoom, pan, mirror, or rotation");
            }
            auto previous = geometry;
            geometry = OnionSliceGeometry(nicxlive::core::Rect{0, 0, 100, 100}, captureCamera,
                capturedRender, Mat4::translation(120.0f, 80.0f, 0));
            check(nearVec2(xy(point(previous.matrix(captureCamera), Vec4{area.x, area.y, 0, 1})),
                    xy(point(captureCamera, Vec4{worldMin.x, worldMin.y, 0, 1}))),
                "capturing another pose must not move or resize the previous onion image");
        }
    }
}
