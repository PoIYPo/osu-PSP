#include <algorithm>
#include <string>

#include "render/glyphs.h"
#include "render/skin.h"
#include "test_framework.h"

using namespace osu;

TEST_CASE(default_skin_has_expected_dimensions) {
    const Skin skin = make_default_skin(128);
    CHECK_EQ(skin.size, 128);
    CHECK_EQ(skin.hit_circle.width, 128);
    CHECK_EQ(skin.hit_circle.height, 128);
    CHECK_EQ(skin.approach_circle.width, 128);
    CHECK_EQ(skin.cursor.width, 128);
    CHECK_EQ(skin.slider_dot.width, 128);
    CHECK(!skin.hit_circle.pixels.empty());
}
TEST_CASE(skin_textures_are_transparent_outside_the_shape) {
    const Skin skin = make_default_skin(128);
    const auto alpha_at = [](const Image& img, int x, int y) {
        return static_cast<int>((img.pixels[static_cast<size_t>(y) * static_cast<size_t>(img.width) +
                                             static_cast<size_t>(x)] >>
                                 24) &
                                0xFFu);
    };

    CHECK(alpha_at(skin.hit_circle, 64, 64) > 200);  // centre is opaque
    CHECK_EQ(alpha_at(skin.hit_circle, 0, 0), 0);    // corner is empty
    CHECK_EQ(alpha_at(skin.hit_circle, 127, 127), 0);

    // The approach texture is a ring: hollow centre, opaque rim somewhere along
    // the vertical axis (the exact rim pixel depends on the thickness).
    CHECK(alpha_at(skin.approach_circle, 64, 64) < 40);
    int rim_alpha = 0;
    for (int y = 0; y < 12; ++y) rim_alpha = std::max(rim_alpha, alpha_at(skin.approach_circle, 64, y));
    CHECK_MSG(rim_alpha > 100, std::to_string(rim_alpha));
}

TEST_CASE(glyph_lookup_includes_latin) {
    CHECK(glyph_rows('0') != nullptr);
    CHECK(glyph_rows('A') != nullptr);
    CHECK(glyph_rows('Z') != nullptr);
    CHECK(glyph_rows('-') != nullptr);
    CHECK(glyph_rows(' ') != nullptr);
    CHECK(glyph_rows('@') == nullptr);
    // Lowercase falls back to the uppercase shape.
    CHECK(glyph_rows('a') == glyph_rows('A'));
}

TEST_CASE(smaller_skin_still_contains_the_shapes) {
    const Skin skin = make_default_skin(96);
    CHECK_EQ(skin.hit_circle.width, 96);
    const size_t centre = static_cast<size_t>(48) * 96u + 48u;
    CHECK(((skin.hit_circle.pixels[centre] >> 24) & 0xFFu) > 200u);
}
