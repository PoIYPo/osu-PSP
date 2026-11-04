#include <string>

#include "core/beatmap.h"
#include "test_framework.h"

using namespace osu;

namespace {

const char* kMap = R"(osu file format v14

[General]
AudioFilename: audio.mp3
AudioLeadIn: 0
PreviewTime: 12345
Countdown: 0
SampleSet: Normal
StackLeniency: 0.7
Mode: 0
LetterboxInBreaks: 0
WidescreenStoryboard: 1

[Editor]
DistanceSpacing: 1.2
BeatDivisor: 4
GridSize: 4
TimelineZoom: 1

[Metadata]
Title:Test Song
TitleUnicode:Тест
Artist:Test Artist
ArtistUnicode:Артист
Creator:Mapper
Version:Insane
Source:
Tags:tag1 tag2
BeatmapID:123
BeatmapSetID:456

[Difficulty]
HPDrainRate:5
CircleSize:4
OverallDifficulty:8
ApproachRate:9
SliderMultiplier:1.4
SliderTickRate:1

[Events]
//Background and Video events
0,0,"bg.jpg",0,0
//Break Periods

[Colours]
Combo1 : 255,0,0

[TimingPoints]
0,500,4,2,0,60,1,0
1000,-50,4,2,0,60,0,0

[HitObjects]
100,100,1000,1,0,0:0:0:0:
200,150,1500,6,0,L|300:150,1,100
256,192,2000,12,0,3000
)";

}  // namespace

TEST_CASE(parses_metadata_and_difficulty) {
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(kMap, bm, &err), err);

    CHECK_EQ(bm.format_version, 14);
    CHECK_STR_EQ(bm.audio_filename, "audio.mp3");
    CHECK_EQ(bm.preview_time, 12345);
    CHECK_EQ(bm.mode, GameMode::Standard);
    CHECK(bm.widescreen_storyboard);

    CHECK_STR_EQ(bm.title, "Test Song");
    CHECK_STR_EQ(bm.title_unicode, "Тест");  // UTF-8 must survive untouched
    CHECK_STR_EQ(bm.creator, "Mapper");
    CHECK_STR_EQ(bm.version, "Insane");
    CHECK_STR_EQ(bm.beatmap_id, "123");

    CHECK_NEAR(bm.circle_size, 4.0, 1e-6);
    CHECK_NEAR(bm.overall_difficulty, 8.0, 1e-6);
    CHECK_NEAR(bm.approach_rate, 9.0, 1e-6);
    CHECK_NEAR(bm.slider_multiplier, 1.4, 1e-6);

    CHECK_STR_EQ(bm.background_file, "bg.jpg");
}

TEST_CASE(parses_timing_points) {
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(kMap, bm, &err), err);

    CHECK_EQ(bm.timing_points.size(), 2u);
    if (bm.timing_points.size() == 2) {
        CHECK_NEAR(bm.timing_points[0].time, 0.0, 1e-9);
        CHECK_NEAR(bm.timing_points[0].beat_length, 500.0, 1e-9);
        CHECK(bm.timing_points[0].uninherited);
        CHECK_EQ(bm.timing_points[0].volume, 60);

        CHECK(bm.timing_points[1].time == 1000.0);
        CHECK(!bm.timing_points[1].uninherited);
        CHECK_NEAR(bm.timing_points[1].slider_velocity_multiplier(), 2.0, 1e-9);
    }
}

TEST_CASE(parses_hit_objects) {
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(kMap, bm, &err), err);

    CHECK_EQ(bm.objects.size(), 3u);
    if (bm.objects.size() != 3) return;

    const HitObject& circle = bm.objects[0];
    CHECK(circle.kind == ObjectKind::Circle);
    CHECK_NEAR(circle.pos.x, 100.0, 1e-6);
    CHECK_NEAR(circle.pos.y, 100.0, 1e-6);
    CHECK_NEAR(circle.time, 1000.0, 1e-9);
    CHECK_NEAR(circle.end_time, 1000.0, 1e-9);
    CHECK(!circle.new_combo);

    const HitObject& slider = bm.objects[1];
    CHECK(slider.kind == ObjectKind::Slider);
    CHECK(slider.new_combo);  // type 6 = slider | new combo
    CHECK_NEAR(slider.time, 1500.0, 1e-9);
    CHECK(slider.slider.curve == CurveType::Linear);
    CHECK_NEAR(slider.slider.pixel_length, 100.0, 1e-6);
    CHECK_EQ(slider.slider.repeats, 1);
    CHECK_EQ(slider.slider.control_points.size(), 2u);  // head + one anchor
    if (slider.slider.control_points.size() == 2) {
        CHECK_NEAR(slider.slider.control_points[0].x, 200.0, 1e-6);
        CHECK_NEAR(slider.slider.control_points[1].x, 300.0, 1e-6);
    }

    const HitObject& spinner = bm.objects[2];
    CHECK(spinner.kind == ObjectKind::Spinner);
    CHECK(spinner.new_combo);
    CHECK_NEAR(spinner.time, 2000.0, 1e-9);
    CHECK_NEAR(spinner.end_time, 3000.0, 1e-9);
}

TEST_CASE(accepts_bom_and_crlf) {
    std::string text = "\xEF\xBB\xBFosu file format v14\r\n\r\n[General]\r\nMode: 0\r\n"
                       "\r\n[TimingPoints]\r\n0,500,4,2,0,60,1,0\r\n"
                       "\r\n[HitObjects]\r\n100,100,1000,1,0\r\n";
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(text, bm, &err), err);
    CHECK_EQ(bm.format_version, 14);
    CHECK_EQ(bm.timing_points.size(), 1u);
    CHECK_EQ(bm.objects.size(), 1u);
    CHECK_STR_EQ(bm.audio_filename, "");
}

TEST_CASE(beats_per_measure_and_volume_defaults) {
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(kMap, bm, &err), err);
    if (!bm.timing_points.empty()) CHECK_EQ(bm.timing_points[0].meter, 4);
}

TEST_CASE(rejects_malformed_hit_object) {
    const char* bad = "osu file format v14\n[TimingPoints]\n0,500,4,2,0,60,1,0\n[HitObjects]\nnope\n";
    Beatmap bm;
    std::string err;
    CHECK(!parse_osu(bad, bm, &err));
    CHECK_MSG(err.find("line 5") != std::string::npos, err);
}

TEST_CASE(rejects_map_without_timing_points) {
    const char* bad = "osu file format v14\n[HitObjects]\n100,100,1000,1,0\n";
    Beatmap bm;
    std::string err;
    CHECK(!parse_osu(bad, bm, &err));
    CHECK_STR_EQ(err, "no timing points");
}

TEST_CASE(old_format_slider_degrades_to_circle) {
    // Pre-v5 sliders have no curve data; better to play them as circles than to
    // refuse the whole map.
    const char* old = "osu file format v4\n[TimingPoints]\n0,500,4,2,0,60,1,0\n[HitObjects]\n100,100,1000,2,0\n";
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(old, bm, &err), err);
    CHECK_EQ(bm.objects.size(), 1u);
    if (!bm.objects.empty()) CHECK(bm.objects[0].kind == ObjectKind::Circle);
}

TEST_CASE(sorts_objects_and_timing_points) {
    const char* text =
        "osu file format v14\n[TimingPoints]\n2000,400,4,2,0,60,1,0\n0,500,4,2,0,60,1,0\n"
        "[HitObjects]\n100,100,2000,1,0\n50,50,1000,1,0\n";
    Beatmap bm;
    std::string err;
    CHECK_MSG(parse_osu(text, bm, &err), err);
    CHECK_EQ(bm.objects.size(), 2u);
    if (bm.objects.size() == 2) CHECK(bm.objects[0].time < bm.objects[1].time);
    if (bm.timing_points.size() == 2) CHECK(bm.timing_points[0].time < bm.timing_points[1].time);
}
