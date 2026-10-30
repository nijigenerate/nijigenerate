// Complete existing regression.d20359-20396 only.
void testCoreMathTriangleInvariants() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec4;
    using nijigenerate::core::math::mesh::MeshVertex;
    namespace triangle = nijigenerate::core::math::triangle;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    // These are the original value structs, not allocated MeshVertex pointers.
    const auto vertex = [](Vec2 position) { MeshVertex result; result.position = position; return result; };
    nijigenerate::compat::Slice<MeshVertex> vertices{
        vertex({0, 0}), vertex({10, 0}), vertex({10, 10}), vertex({0, 10}), vertex({5, 5})};
    nijigenerate::compat::Slice<Vec2> positions;
    for (auto value : vertices) positions.append(value.position);
    auto bounds = triangle::getBounds(positions);
    check(bounds == Vec4{0, 0, 10, 10}, "triangle getBounds should floor/ceil vertex bounds");
    // The existing production reader entry is the same triangulateVertices body
    // as its pointer/position ingress, while retaining the original value array.
    auto triResult = triangle::triangulatePositions(vertices.size(),
        [&](std::size_t i) { return vertices[i].position; }, Vec4{0, 0, 10, 10});
    auto triangulatedVertices = std::get<0>(triResult);
    auto tris = std::get<1>(triResult);
    check(triangulatedVertices.size() == vertices.size(),
        "triangulate should preserve original vertices after trimming helper vertices");
    check(tris.size() > 0, "triangulate should produce triangles for a square with center point");
    for (auto tri : tris) {
        check(tri.x < triangulatedVertices.size() && tri.y < triangulatedVertices.size() && tri.z < triangulatedVertices.size(),
            "triangle indices should stay in range");
        check(tri.x != tri.y && tri.y != tri.z && tri.z != tri.x, "triangle should not repeat vertices");
        const Vec2 a = triangulatedVertices[tri.x];
        const Vec2 b = triangulatedVertices[tri.y];
        const Vec2 c = triangulatedVertices[tri.z];
        auto twiceArea = std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
        check(twiceArea > 0.0001f, "triangle should have non-zero area");
    }
    nijigenerate::compat::Slice<std::int32_t> texture;
    texture.resize(121);
    triangle::fillPoly(texture, 11, 11, Vec4{0, 0, 10, 10}, triangulatedVertices.toArray(), tris, 0, 7);
    bool contains = false;
    for (auto pixel : texture) if (pixel == 7) { contains = true; break; }
    check(contains, "fillPoly should rasterize at least one pixel for a valid triangle");
}
