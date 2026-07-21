#include "test_framework.hpp"

#include "stream_pack.hpp"
#include "ui_audio_pack.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace {

using namespace arpg::platform;

struct FakeStreams final {
    int fail{-1};
    std::array<int,4> loads{}, plays{}, updates{}, stops{}, unloads{};
    std::array<bool,4> playing{};
    std::array<float,4> volume{};
};
FakeStreams* g_streams{};

int stream_id(const char* path) noexcept {
    if (std::strstr(path,"music-explore")) return 0;
    if (std::strstr(path,"music-combat")) return 1;
    if (std::strstr(path,"ambience-room")) return 2;
    return 3;
}
Music fake_load_music(const char* path) noexcept {
    const int id=stream_id(path); ++g_streams->loads[id]; Music result{}; result.ctxType=id;
    if(id!=g_streams->fail) result.ctxData=reinterpret_cast<void*>(static_cast<std::uintptr_t>(id+1));
    return result;
}
bool fake_music_valid(Music music) noexcept { return music.ctxData!=nullptr; }
void fake_unload_music(Music music) noexcept { ++g_streams->unloads[music.ctxType]; }
void fake_play_music(Music music) noexcept { ++g_streams->plays[music.ctxType]; g_streams->playing[music.ctxType]=true; }
bool fake_music_playing(Music music) noexcept { return g_streams->playing[music.ctxType]; }
void fake_update_music(Music music) noexcept { ++g_streams->updates[music.ctxType]; }
void fake_stop_music(Music music) noexcept { ++g_streams->stops[music.ctxType]; g_streams->playing[music.ctxType]=false; }
void fake_music_volume(Music music,float value) noexcept { g_streams->volume[music.ctxType]=value; }
MusicStreamApi fake_stream_api() noexcept { return {&fake_load_music,&fake_music_valid,&fake_unload_music,&fake_play_music,&fake_music_playing,&fake_update_music,&fake_stop_music,&fake_music_volume}; }

struct FakeUi final {
    int fail_wave{-1}; bool fail_fallback{};
    int wave_unloads{}, sound_loads{}, sound_unloads{}, plays{}, stops{}, volumes{};
    std::array<bool,6> external{};
    float last_volume{};
};
FakeUi* g_ui{};
int ui_id(const char* path) noexcept {
    if(std::strstr(path,"navigate"))return 0; if(std::strstr(path,"confirm"))return 1;
    if(std::strstr(path,"cancel"))return 2; if(std::strstr(path,"open"))return 3;
    if(std::strstr(path,"close"))return 4; return 5;
}
Wave fake_load_wave(const char* path) noexcept {
    const int id=ui_id(path); Wave wave{}; if(id==g_ui->fail_wave)return wave;
    wave.frameCount=100; wave.sampleRate=44100; wave.sampleSize=16; wave.channels=1;
    wave.data=reinterpret_cast<void*>(static_cast<std::uintptr_t>(id+1)); g_ui->external[id]=true; return wave;
}
bool fake_wave_valid(Wave wave) noexcept { return wave.data!=nullptr; }
Sound fake_load_sound(Wave wave) noexcept {
    ++g_ui->sound_loads; Sound sound{};
    const bool fallback=wave.sampleRate==22050U;
    if(!(fallback&&g_ui->fail_fallback)) sound.stream.buffer=reinterpret_cast<rAudioBuffer*>(wave.data);
    return sound;
}
bool fake_sound_valid(Sound sound) noexcept { return sound.stream.buffer!=nullptr; }
void fake_unload_wave(Wave) noexcept { ++g_ui->wave_unloads; }
void fake_unload_sound(Sound) noexcept { ++g_ui->sound_unloads; }
void fake_play_sound(Sound) noexcept { ++g_ui->plays; }
void fake_stop_sound(Sound) noexcept { ++g_ui->stops; }
void fake_sound_volume(Sound,float value) noexcept { ++g_ui->volumes; g_ui->last_volume=value; }
UiSoundApi fake_ui_api() noexcept { return {&fake_load_wave,&fake_wave_valid,&fake_load_sound,&fake_sound_valid,&fake_unload_wave,&fake_unload_sound,&fake_play_sound,&fake_stop_sound,&fake_sound_volume}; }

arpg::test::Failure streams_load_play_update_and_clamp() noexcept {
    FakeStreams fake{}; g_streams=&fake; StreamPack pack{fake_stream_api()};
    ARPG_REQUIRE(pack.load());
    for(int i=0;i<4;++i) ARPG_REQUIRE(pack.available(static_cast<StreamAssetId>(i)));
    pack.play(StreamAssetId::music_explore); pack.play(StreamAssetId::music_explore); pack.update();
    pack.set_volume(StreamAssetId::music_explore,2.0F);
    ARPG_REQUIRE(fake.plays[0]==1 && fake.updates[0]==1 && arpg::test::near(fake.volume[0],1.0));
    return {};
}
arpg::test::Failure missing_stream_is_local_silence() noexcept {
    FakeStreams fake{}; fake.fail=1; g_streams=&fake; StreamPack pack{fake_stream_api()};
    ARPG_REQUIRE(pack.load()); ARPG_REQUIRE(!pack.available(StreamAssetId::music_combat));
    ARPG_REQUIRE(pack.available(StreamAssetId::music_explore)); ARPG_REQUIRE(pack.available(StreamAssetId::ambience_abyss));
    return {};
}
arpg::test::Failure streams_stop_and_unload_once() noexcept {
    FakeStreams fake{}; g_streams=&fake; StreamPack pack{fake_stream_api()}; ARPG_REQUIRE(pack.load());
    pack.play(StreamAssetId::ambience_room); pack.stop_all(); pack.unload(); pack.unload();
    ARPG_REQUIRE(fake.stops[2]==1); for(int value:fake.unloads) ARPG_REQUIRE(value==1); return {};
}
arpg::test::Failure incomplete_stream_api_is_rejected() noexcept {
    MusicStreamApi api=fake_stream_api(); api.update=nullptr; StreamPack pack{api}; ARPG_REQUIRE(!pack.load()); return {};
}
arpg::test::Failure ui_pack_loads_six_external_waves() noexcept {
    FakeUi fake{}; g_ui=&fake; UiAudioPack pack{fake_ui_api()}; ARPG_REQUIRE(pack.load());
    for(int i=0;i<6;++i){const auto cue=static_cast<UiAudioCue>(i);ARPG_REQUIRE(pack.available(cue));ARPG_REQUIRE(!pack.using_fallback(cue));}
    ARPG_REQUIRE(fake.wave_unloads==6); return {};
}
arpg::test::Failure one_missing_ui_wave_uses_one_fallback() noexcept {
    FakeUi fake{}; fake.fail_wave=2; g_ui=&fake; UiAudioPack pack{fake_ui_api()}; ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(pack.using_fallback(UiAudioCue::cancel)); ARPG_REQUIRE(!pack.using_fallback(UiAudioCue::confirm)); return {};
}
arpg::test::Failure failed_ui_fallback_rolls_back() noexcept {
    FakeUi fake{}; fake.fail_wave=2; fake.fail_fallback=true; g_ui=&fake; UiAudioPack pack{fake_ui_api()};
    ARPG_REQUIRE(!pack.load()); for(int i=0;i<6;++i) ARPG_REQUIRE(!pack.available(static_cast<UiAudioCue>(i)));
    ARPG_REQUIRE(fake.sound_unloads==2); return {};
}
arpg::test::Failure ui_play_volume_stop_and_unload_are_bounded() noexcept {
    FakeUi fake{}; g_ui=&fake; UiAudioPack pack{fake_ui_api()}; ARPG_REQUIRE(pack.load());
    pack.play(UiAudioCue::reward); pack.set_volume(-1.0F); pack.stop_all(); pack.unload(); pack.unload();
    ARPG_REQUIRE(fake.plays==1 && fake.volumes==6 && arpg::test::near(fake.last_volume,0.0));
    ARPG_REQUIRE(fake.stops==6 && fake.sound_unloads==6); return {};
}

constexpr arpg::test::TestCase kCases[]={{"stream lifecycle",&streams_load_play_update_and_clamp},{"stream local silence",&missing_stream_is_local_silence},{"stream unload",&streams_stop_and_unload_once},{"stream API validation",&incomplete_stream_api_is_rejected},{"UI external load",&ui_pack_loads_six_external_waves},{"UI one fallback",&one_missing_ui_wave_uses_one_fallback},{"UI fallback rollback",&failed_ui_fallback_rolls_back},{"UI playback lifecycle",&ui_play_volume_stop_and_unload_are_bounded}};
}

arpg::test::TestSuite stage15_audio_pack_suite() noexcept { return arpg::test::make_suite("stage15_audio_pack",kCases); }
