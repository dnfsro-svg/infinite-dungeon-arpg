#include "material_pack.hpp"

#include "material_asset_validation.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

namespace arpg::platform {
namespace {

void saturating_increment(std::uint64_t& value) noexcept {
    if (value != (std::numeric_limits<std::uint64_t>::max)()) ++value;
}

[[nodiscard]] constexpr bool is_known_atlas(MaterialAtlasId id) noexcept {
    return id < MaterialAtlasId::count;
}

[[nodiscard]] constexpr bool is_known_sprite(MaterialSpriteId id) noexcept {
    return id > MaterialSpriteId::missing && id < MaterialSpriteId::count;
}

[[nodiscard]] constexpr std::size_t atlas_index(MaterialAtlasId id) noexcept {
    return static_cast<std::size_t>(id);
}

[[nodiscard]] constexpr MaterialAtlasId atlas_for_sprite(
    MaterialSpriteId id) noexcept {
    if (id >= MaterialSpriteId::item_weapon
            && id <= MaterialSpriteId::bag_frame_se) {
        return MaterialAtlasId::items_ui;
    }
    if (id >= MaterialSpriteId::ui_hud_panel
            && id <= MaterialSpriteId::ui_reinforcement_cancel) {
        return MaterialAtlasId::ui_material;
    }
    switch (id) {
    case MaterialSpriteId::environment_door_fire:
    case MaterialSpriteId::environment_door_water:
    case MaterialSpriteId::environment_door_lightning:
    case MaterialSpriteId::environment_door_chaos:
        return MaterialAtlasId::element_doors;
    case MaterialSpriteId::environment_floor_fire:
    case MaterialSpriteId::environment_hole:
        return MaterialAtlasId::environment;
    case MaterialSpriteId::environment_floor_water:
    case MaterialSpriteId::water_wall:
    case MaterialSpriteId::water_hole:
    case MaterialSpriteId::water_lantern:
    case MaterialSpriteId::water_coral:
    case MaterialSpriteId::water_grate:
        return MaterialAtlasId::water_environment;
    case MaterialSpriteId::environment_floor_lightning:
    case MaterialSpriteId::lightning_wall:
    case MaterialSpriteId::lightning_hole:
    case MaterialSpriteId::lightning_arc_lamp:
    case MaterialSpriteId::lightning_capacitor_bank:
    case MaterialSpriteId::lightning_grounding_rod:
        return MaterialAtlasId::lightning_environment;
    case MaterialSpriteId::environment_floor_chaos:
    case MaterialSpriteId::chaos_wall:
    case MaterialSpriteId::chaos_hole:
    case MaterialSpriteId::chaos_rift_lantern:
    case MaterialSpriteId::chaos_anomaly_condenser:
    case MaterialSpriteId::chaos_warning_obelisk:
        return MaterialAtlasId::chaos_environment;
    case MaterialSpriteId::fire_wall:
    case MaterialSpriteId::fire_torch:
    case MaterialSpriteId::fire_chain:
    case MaterialSpriteId::fire_banner:
    case MaterialSpriteId::fire_weapon_rack:
    case MaterialSpriteId::fire_bone_pile:
    case MaterialSpriteId::fire_breakable_crate:
    case MaterialSpriteId::fire_solid_brazier:
        return MaterialAtlasId::fire_environment;
    case MaterialSpriteId::fire_bomber_idle:
    case MaterialSpriteId::fire_bomber_move:
    case MaterialSpriteId::fire_bomber_telegraph:
    case MaterialSpriteId::fire_bomber_active:
    case MaterialSpriteId::fire_bomber_recovery:
    case MaterialSpriteId::fire_bomber_cooldown:
    case MaterialSpriteId::fire_bomber_defeated:
        return MaterialAtlasId::fire_bomber;
    case MaterialSpriteId::fire_charger_idle:
    case MaterialSpriteId::fire_charger_move:
    case MaterialSpriteId::fire_charger_telegraph:
    case MaterialSpriteId::fire_charger_active:
    case MaterialSpriteId::fire_charger_recovery:
    case MaterialSpriteId::fire_charger_cooldown:
    case MaterialSpriteId::fire_charger_defeated:
        return MaterialAtlasId::fire_charger;
    case MaterialSpriteId::water_bulwark_idle:
    case MaterialSpriteId::water_bulwark_move:
    case MaterialSpriteId::water_bulwark_telegraph:
    case MaterialSpriteId::water_bulwark_active:
    case MaterialSpriteId::water_bulwark_recovery:
    case MaterialSpriteId::water_bulwark_cooldown:
    case MaterialSpriteId::water_bulwark_defeated:
        return MaterialAtlasId::water_bulwark;
    case MaterialSpriteId::water_support_idle:
    case MaterialSpriteId::water_support_move:
    case MaterialSpriteId::water_support_telegraph:
    case MaterialSpriteId::water_support_active:
    case MaterialSpriteId::water_support_recovery:
    case MaterialSpriteId::water_support_cooldown:
    case MaterialSpriteId::water_support_defeated:
        return MaterialAtlasId::water_support;
    case MaterialSpriteId::lightning_shooter_idle:
    case MaterialSpriteId::lightning_shooter_move:
    case MaterialSpriteId::lightning_shooter_telegraph:
    case MaterialSpriteId::lightning_shooter_active:
    case MaterialSpriteId::lightning_shooter_recovery:
    case MaterialSpriteId::lightning_shooter_cooldown:
    case MaterialSpriteId::lightning_shooter_defeated:
        return MaterialAtlasId::lightning_shooter;
    case MaterialSpriteId::lightning_dasher_idle:
    case MaterialSpriteId::lightning_dasher_move:
    case MaterialSpriteId::lightning_dasher_telegraph:
    case MaterialSpriteId::lightning_dasher_active:
    case MaterialSpriteId::lightning_dasher_recovery:
    case MaterialSpriteId::lightning_dasher_cooldown:
    case MaterialSpriteId::lightning_dasher_defeated:
        return MaterialAtlasId::lightning_dasher;
    case MaterialSpriteId::chaos_chaser_idle:
    case MaterialSpriteId::chaos_chaser_move:
    case MaterialSpriteId::chaos_chaser_telegraph:
    case MaterialSpriteId::chaos_chaser_active:
    case MaterialSpriteId::chaos_chaser_recovery:
    case MaterialSpriteId::chaos_chaser_cooldown:
    case MaterialSpriteId::chaos_chaser_defeated:
        return MaterialAtlasId::chaos_chaser;
    case MaterialSpriteId::chaos_hazard_idle:
    case MaterialSpriteId::chaos_hazard_move:
    case MaterialSpriteId::chaos_hazard_telegraph:
    case MaterialSpriteId::chaos_hazard_active:
    case MaterialSpriteId::chaos_hazard_recovery:
    case MaterialSpriteId::chaos_hazard_cooldown:
    case MaterialSpriteId::chaos_hazard_defeated:
        return MaterialAtlasId::chaos_hazard;
    case MaterialSpriteId::effect_fire:
    case MaterialSpriteId::effect_water:
    case MaterialSpriteId::effect_lightning:
    case MaterialSpriteId::effect_chaos:
    case MaterialSpriteId::effect_hit_spark:
    case MaterialSpriteId::effect_launcher_trail:
    case MaterialSpriteId::effect_landing_dust:
    case MaterialSpriteId::effect_affix_aura:
        return MaterialAtlasId::effects_ui;
    default:
        break;
    }
    return MaterialAtlasId::actors;
}

[[nodiscard]] const MaterialFrameDefinition* find_frame(
    const MaterialManifestDefinition& manifest, MaterialSpriteId id) noexcept {
    for (std::size_t index = 0U; index < manifest.frame_count; ++index) {
        if (manifest.frames[index].id == id) return &manifest.frames[index];
    }
    return nullptr;
}

[[nodiscard]] constexpr bool valid_texture_api(
    MaterialTextureApi texture_api) noexcept {
    return texture_api.load != nullptr && texture_api.valid != nullptr
        && texture_api.unload != nullptr
        && texture_api.initialize_material_pipeline != nullptr
        && texture_api.shutdown_material_pipeline != nullptr
        && texture_api.draw_material != nullptr;
}

[[nodiscard]] constexpr Vector2 panel_background_sample(
    MaterialSpriteId id) noexcept {
    switch (id) {
    case MaterialSpriteId::ui_inventory_panel_equipment: return {88.0F, 44.0F};
    case MaterialSpriteId::ui_inventory_panel_grid: return {88.0F, 48.0F};
    case MaterialSpriteId::ui_inventory_panel_detail: return {40.0F, 68.0F};
    case MaterialSpriteId::ui_skill_panel: return {88.0F, 88.0F};
    case MaterialSpriteId::ui_pause_panel: return {84.0F, 56.0F};
    case MaterialSpriteId::ui_warning_modal: return {64.0F, 64.0F};
    default: return {40.0F, 40.0F};
    }
}

struct MaterialShaderState final {
    Shader shader{};
    int material_map_location{-1};
    int channel_map_location{-1};
    int channel_strengths_location{-1};
    int emissive_tint_location{-1};
    bool ready{};
};

MaterialShaderState g_material_shader{};

constexpr const char* kMaterialFragmentShader = R"glsl(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D materialMap;
uniform vec3 channelMap;
uniform vec3 channelStrengths;
uniform vec3 emissiveTint;
out vec4 finalColor;
float packedChannel(vec4 materialSample, float channel) {
    if (channel < 0.5) return materialSample.r;
    if (channel < 1.5) return materialSample.g;
    if (channel < 2.5) return materialSample.b;
    return materialSample.a;
}
void main() {
    vec4 base = texture(texture0, fragTexCoord) * fragColor;
    vec4 materialSample = texture(materialMap, fragTexCoord);
    float roughness = clamp(packedChannel(materialSample, channelMap.x)
        * channelStrengths.x, 0.0, 1.0);
    float emissive = clamp(packedChannel(materialSample, channelMap.y)
        * channelStrengths.y, 0.0, 1.0);
    float metalness = clamp(packedChannel(materialSample, channelMap.z)
        * channelStrengths.z, 0.0, 1.0);
    float diffuseLight = 1.0 - 0.42 * roughness;
    vec3 metalLight = metalness * (base.rgb * 0.24 + vec3(0.08, 0.10, 0.13));
    vec3 emissiveLight = emissiveTint * emissive;
    finalColor = vec4(base.rgb * diffuseLight + metalLight + emissiveLight,
        base.a * materialSample.a);
}
)glsl";

bool initialize_material_pipeline() noexcept {
    if (g_material_shader.ready) return true;
    g_material_shader.shader = LoadShaderFromMemory(
        nullptr, kMaterialFragmentShader);
    if (!IsShaderValid(g_material_shader.shader)) return false;
    g_material_shader.material_map_location = GetShaderLocation(
        g_material_shader.shader, "materialMap");
    g_material_shader.channel_map_location = GetShaderLocation(
        g_material_shader.shader, "channelMap");
    g_material_shader.channel_strengths_location = GetShaderLocation(
        g_material_shader.shader, "channelStrengths");
    g_material_shader.emissive_tint_location = GetShaderLocation(
        g_material_shader.shader, "emissiveTint");
    g_material_shader.ready = g_material_shader.material_map_location >= 0
        && g_material_shader.channel_map_location >= 0
        && g_material_shader.channel_strengths_location >= 0
        && g_material_shader.emissive_tint_location >= 0;
    if (!g_material_shader.ready) {
        UnloadShader(g_material_shader.shader);
        g_material_shader = {};
    }
    return g_material_shader.ready;
}

void shutdown_material_pipeline() noexcept {
    if (g_material_shader.ready) UnloadShader(g_material_shader.shader);
    g_material_shader = {};
}

void draw_material(Texture2D color, Texture2D material, Rectangle source,
    Rectangle destination, Vector2 origin, float rotation, Color tint,
    MaterialCompositeParameters parameters) noexcept {
    if (!g_material_shader.ready) return;
    const float channel_strengths[3]{parameters.roughness_strength,
        parameters.emissive_strength, parameters.metalness_strength};
    const float channel_map[3]{
        static_cast<float>(parameters.roughness_channel),
        static_cast<float>(parameters.emissive_channel),
        static_cast<float>(parameters.metalness_channel)};
    const float emissive_tint[3]{
        static_cast<float>(parameters.emissive_tint.r) / 255.0F,
        static_cast<float>(parameters.emissive_tint.g) / 255.0F,
        static_cast<float>(parameters.emissive_tint.b) / 255.0F};
    SetShaderValueTexture(g_material_shader.shader,
        g_material_shader.material_map_location, material);
    SetShaderValue(g_material_shader.shader,
        g_material_shader.channel_map_location, channel_map,
        SHADER_UNIFORM_VEC3);
    SetShaderValue(g_material_shader.shader,
        g_material_shader.channel_strengths_location, channel_strengths,
        SHADER_UNIFORM_VEC3);
    SetShaderValue(g_material_shader.shader,
        g_material_shader.emissive_tint_location, emissive_tint,
        SHADER_UNIFORM_VEC3);
    BeginShaderMode(g_material_shader.shader);
    DrawTexturePro(color, source, destination, origin, rotation, tint);
    EndShaderMode();
}

[[nodiscard]] MaterialTextureApi default_material_texture_api() noexcept {
    return {&LoadTexture, &IsTextureValid, &UnloadTexture,
        &initialize_material_pipeline, &shutdown_material_pipeline,
        &draw_material};
}

[[nodiscard]] constexpr bool ecology_is_valid(
    MaterialEcology ecology) noexcept {
    return ecology < MaterialEcology::count;
}

[[nodiscard]] constexpr bool atlas_required(
    const MaterialAtlasDefinition& atlas, MaterialEcology ecology) noexcept {
    return atlas.ecology == MaterialEcology::common
        || atlas.ecology == ecology;
}

}  // namespace

MaterialPack::MaterialPack() noexcept
    : MaterialPack(default_material_texture_api()) {}

MaterialPack::MaterialPack(MaterialTextureApi texture_api) noexcept
    : texture_api_(texture_api) {}

void MaterialPackState::set_available(MaterialAtlasId id, bool available) noexcept {
    if (!is_known_atlas(id)) return;
    available_[atlas_index(id)] = available;
}

bool MaterialPackState::available(MaterialAtlasId id) const noexcept {
    return is_known_atlas(id) && available_[atlas_index(id)];
}

bool MaterialPackState::can_draw(MaterialSpriteId id) const noexcept {
    return is_known_sprite(id) && available(atlas_for_sprite(id));
}

bool MaterialPackState::any_available() const noexcept {
    for (const bool available : available_) {
        if (available) return true;
    }
    return false;
}

void MaterialPackState::reset() noexcept {
    available_.fill(false);
}

bool MaterialPack::load(MaterialEcology ecology) noexcept {
    if (!ecology_is_valid(ecology)) return false;
    const MaterialManifestDefinition manifest = default_material_manifest();
    MaterialResidencyRequest request = base_material_residency_request();
    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& definition = manifest.atlases[index];
        if (atlas_required(definition, ecology)) request.require(definition.id);
    }
    static_cast<void>(synchronize_residency(request));
    current_ecology_ = ecology;
    return state_.any_available();
}

bool MaterialPack::synchronize_residency(
    MaterialResidencyRequest request) noexcept {
    if (request == requested_residency_) return residency_satisfied(request);
    requested_residency_ = request;
    if (!valid_texture_api(texture_api_)) {
        TraceLog(LOG_WARNING,
            "Stage 12 material texture API is incomplete; using program fallback");
        return false;
    }
    const MaterialManifestDefinition manifest = default_material_manifest();
    if (!validate_material_manifest(manifest).valid) {
        TraceLog(LOG_WARNING,
            "Stage 12 material manifest is invalid; using program fallback");
        return false;
    }
    const std::size_t target_bytes = material_residency_bytes(manifest, request);
    const std::size_t transition_bytes = material_residency_bytes(manifest,
        {resident_atlases_ | request.atlases});
    if (target_bytes > manifest.memory_budget_bytes
        || transition_bytes > manifest.memory_budget_bytes) {
        return false;
    }
    if (!material_pipeline_ready_) {
        material_pipeline_ready_ = texture_api_.initialize_material_pipeline();
        if (!material_pipeline_ready_) {
            TraceLog(LOG_WARNING,
                "Stage 12 material shader pipeline is unavailable; using program fallback");
            return false;
        }
    }

    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& definition = manifest.atlases[index];
        if (!request.contains(definition.id)) continue;
        const std::size_t texture_index = atlas_index(definition.id);
        if (texture_api_.valid(color_textures_[texture_index])
            && texture_api_.valid(material_textures_[texture_index])) {
            state_.set_available(definition.id, true);
            resident_atlases_ |= MaterialAtlasMask{1U} << texture_index;
            continue;
        }
        if (texture_api_.valid(color_textures_[texture_index])) {
            saturating_increment(texture_unload_call_count_);
            texture_api_.unload(color_textures_[texture_index]);
        }
        if (texture_api_.valid(material_textures_[texture_index])) {
            saturating_increment(texture_unload_call_count_);
            texture_api_.unload(material_textures_[texture_index]);
        }
        color_textures_[texture_index] = {};
        material_textures_[texture_index] = {};
        state_.set_available(definition.id, false);
        resident_atlases_ &= ~(MaterialAtlasMask{1U} << texture_index);
        std::array<char, 512> color_deployed_path{};
        std::array<char, 512> material_deployed_path{};
        const int color_written = std::snprintf(color_deployed_path.data(),
            color_deployed_path.size(), "%s%s", GetApplicationDirectory(),
            definition.color_path);
        const int material_written = std::snprintf(material_deployed_path.data(),
            material_deployed_path.size(), "%s%s", GetApplicationDirectory(),
            definition.material_path);
        const char* const color_path = color_written > 0
                && static_cast<std::size_t>(color_written)
                    < color_deployed_path.size()
            ? color_deployed_path.data() : definition.color_path;
        const char* const material_path = material_written > 0
                && static_cast<std::size_t>(material_written)
                    < material_deployed_path.size()
            ? material_deployed_path.data() : definition.material_path;
        saturating_increment(texture_load_call_count_);
        Texture2D color_texture = texture_api_.load(color_path);
        saturating_increment(texture_load_call_count_);
        Texture2D material_texture = texture_api_.load(material_path);
        const bool color_dimensions_match = texture_api_.valid(color_texture)
            && color_texture.width == definition.width
            && color_texture.height == definition.height;
        const bool material_dimensions_match = texture_api_.valid(material_texture)
            && material_texture.width == definition.width
            && material_texture.height == definition.height;
        if (!color_dimensions_match || !material_dimensions_match) {
            if (texture_api_.valid(color_texture)) {
                saturating_increment(texture_unload_call_count_);
                texture_api_.unload(color_texture);
            }
            if (texture_api_.valid(material_texture)) {
                saturating_increment(texture_unload_call_count_);
                texture_api_.unload(material_texture);
            }
            if (!warnings_emitted_[texture_index]) {
                TraceLog(LOG_WARNING,
                    "Stage 12 material atlas %u unavailable: %s | %s; using program fallback",
                    static_cast<unsigned int>(definition.id),
                    definition.color_path, definition.material_path);
                warnings_emitted_[texture_index] = true;
            }
            continue;
        }
        color_textures_[texture_index] = color_texture;
        material_textures_[texture_index] = material_texture;
        state_.set_available(definition.id, true);
        resident_atlases_ |= MaterialAtlasMask{1U} << texture_index;
    }

    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& definition = manifest.atlases[index];
        const std::size_t texture_index = atlas_index(definition.id);
        if (request.contains(definition.id)
            || (resident_atlases_ & (MaterialAtlasMask{1U} << texture_index)) == 0U) {
            continue;
        }
        if (texture_api_.valid(color_textures_[texture_index])) {
            saturating_increment(texture_unload_call_count_);
            texture_api_.unload(color_textures_[texture_index]);
        }
        if (texture_api_.valid(material_textures_[texture_index])) {
            saturating_increment(texture_unload_call_count_);
            texture_api_.unload(material_textures_[texture_index]);
        }
        color_textures_[texture_index] = {};
        material_textures_[texture_index] = {};
        state_.set_available(definition.id, false);
        resident_atlases_ &= ~(MaterialAtlasMask{1U} << texture_index);
    }
    return residency_satisfied(request);
}

bool MaterialPack::residency_satisfied(
    MaterialResidencyRequest request) const noexcept {
    return (resident_atlases_ & request.atlases) == request.atlases;
}

MaterialResidencyRequest MaterialPack::requested_residency() const noexcept {
    return requested_residency_;
}

MaterialAtlasMask MaterialPack::resident_atlases() const noexcept {
    return resident_atlases_;
}

std::size_t MaterialPack::resident_bytes() const noexcept {
    return material_residency_bytes(default_material_manifest(),
        {resident_atlases_});
}

std::uint64_t MaterialPack::texture_load_call_count() const noexcept {
    return texture_load_call_count_;
}

std::uint64_t MaterialPack::texture_unload_call_count() const noexcept {
    return texture_unload_call_count_;
}

void MaterialPack::unload() noexcept {
    for (std::size_t index{}; index < color_textures_.size(); ++index) {
        if (valid_texture_api(texture_api_)
            && texture_api_.valid(color_textures_[index])) {
            saturating_increment(texture_unload_call_count_);
            texture_api_.unload(color_textures_[index]);
        }
        if (valid_texture_api(texture_api_)
            && texture_api_.valid(material_textures_[index])) {
            saturating_increment(texture_unload_call_count_);
            texture_api_.unload(material_textures_[index]);
        }
        color_textures_[index] = Texture2D{};
        material_textures_[index] = Texture2D{};
    }
    state_.reset();
    sprite_draw_counts_.fill(0U);
    direct_stretch_draw_counts_.fill(0U);
    current_ecology_ = MaterialEcology::common;
    requested_residency_ = {};
    resident_atlases_ = {};
    if (material_pipeline_ready_ && valid_texture_api(texture_api_)) {
        texture_api_.shutdown_material_pipeline();
    }
    material_pipeline_ready_ = false;
}

MaterialEcology MaterialPack::current_ecology() const noexcept {
    return current_ecology_;
}

bool MaterialPack::material_pipeline_ready() const noexcept {
    return material_pipeline_ready_;
}

bool MaterialPack::ecology_ready(MaterialEcology ecology) const noexcept {
    if (!material_pipeline_ready_ || current_ecology_ != ecology
        || !ecology_is_valid(ecology) || !valid_texture_api(texture_api_)) {
        return false;
    }
    const MaterialManifestDefinition manifest = default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& definition = manifest.atlases[index];
        if (!atlas_required(definition, ecology)) continue;
        const std::size_t texture_index = atlas_index(definition.id);
        if (!state_.available(definition.id)
            || !texture_api_.valid(color_textures_[texture_index])
            || !texture_api_.valid(material_textures_[texture_index])) {
            return false;
        }
    }
    return true;
}

bool MaterialPack::available(MaterialAtlasId id) const noexcept {
    return state_.available(id);
}

bool MaterialPack::can_draw(MaterialSpriteId id) const noexcept {
    if (!state_.can_draw(id) || !valid_texture_api(texture_api_)) return false;

    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)) return false;

    const std::size_t index = atlas_index(frame->atlas);
    return texture_api_.valid(color_textures_[index])
        && texture_api_.valid(material_textures_[index]);
}

bool MaterialPack::draw(MaterialSpriteId id, Vector2 foot_position,
    bool flip_x, float scale, Color tint) const noexcept {
    if (!can_draw(id) || scale <= 0.0F) return false;

    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)) return false;

    const std::size_t index = atlas_index(frame->atlas);
    const Texture2D& color_texture = color_textures_[index];
    const Texture2D& material_texture = material_textures_[index];

    Rectangle source = frame->source;
    if (flip_x) {
        source.x += source.width;
        source.width = -source.width;
    }
    const Rectangle destination{foot_position.x - frame->foot_anchor.x * scale,
        foot_position.y - frame->foot_anchor.y * scale,
        frame->source.width * scale, frame->source.height * scale};
    texture_api_.draw_material(color_texture, material_texture, source,
        destination, {0.0F, 0.0F}, 0.0F, tint, {});
    ++sprite_draw_counts_[static_cast<std::size_t>(id)];
    return true;
}

bool MaterialPack::draw_to(MaterialSpriteId id, Rectangle destination,
    Color tint) const noexcept {
    if (!can_draw(id) || destination.width <= 0.0F
            || destination.height <= 0.0F) {
        return false;
    }
    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)) return false;
    const std::size_t index = atlas_index(frame->atlas);
    texture_api_.draw_material(color_textures_[index], material_textures_[index],
        frame->source, destination, {0.0F, 0.0F}, 0.0F, tint, {});
    ++sprite_draw_counts_[static_cast<std::size_t>(id)];
    ++direct_stretch_draw_counts_[static_cast<std::size_t>(id)];
    return true;
}

bool MaterialPack::draw_nine_slice(MaterialSpriteId id,
    Rectangle destination, float border_pixels, Color tint) const noexcept {
    if (!can_draw(id) || destination.width <= 0.0F
            || destination.height <= 0.0F || border_pixels <= 0.0F) {
        return false;
    }
    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)
            || border_pixels * 2.0F >= frame->source.width
            || border_pixels * 2.0F >= frame->source.height) {
        return false;
    }
    if (border_pixels != 32.0F || destination.width < 64.0F
            || destination.height < 64.0F || frame->source.width != 128.0F
            || frame->source.height != 128.0F) {
        return false;
    }
    const std::size_t texture_index = atlas_index(frame->atlas);
    const auto draw_part = [&](Rectangle local_source,
                               Rectangle part_destination) noexcept {
        local_source.x += frame->source.x;
        local_source.y += frame->source.y;
        texture_api_.draw_material(color_textures_[texture_index],
            material_textures_[texture_index], local_source, part_destination,
            {0.0F, 0.0F}, 0.0F, tint, {});
    };
    const Vector2 background = panel_background_sample(id);
    draw_part({background.x, background.y, 1.0F, 1.0F},
        {destination.x + 32.0F, destination.y + 32.0F,
            destination.width - 64.0F, destination.height - 64.0F});
    const auto tile_horizontal = [&](Rectangle source, float y,
                                     float width) noexcept {
        float x = destination.x + 32.0F;
        float remaining = width;
        while (remaining > 0.0F) {
            const float segment = std::min(source.width, remaining);
            Rectangle clipped = source;
            clipped.width = segment;
            draw_part(clipped, {x, y, segment, source.height});
            x += segment;
            remaining -= segment;
        }
    };
    const auto tile_vertical = [&](Rectangle source, float x,
                                   float height) noexcept {
        float y = destination.y + 32.0F;
        float remaining = height;
        while (remaining > 0.0F) {
            const float segment = std::min(source.height, remaining);
            Rectangle clipped = source;
            clipped.height = segment;
            draw_part(clipped, {x, y, source.width, segment});
            y += segment;
            remaining -= segment;
        }
    };
    const float inner_width = destination.width - 64.0F;
    const float inner_height = destination.height - 64.0F;
    tile_horizontal({32.0F, 0.0F, 8.0F, 32.0F},
        destination.y, inner_width);
    tile_horizontal({32.0F, 96.0F, 8.0F, 32.0F},
        destination.y + destination.height - 32.0F, inner_width);
    tile_vertical({0.0F, 32.0F, 32.0F, 8.0F},
        destination.x, inner_height);
    tile_vertical({96.0F, 32.0F, 32.0F, 8.0F},
        destination.x + destination.width - 32.0F, inner_height);
    draw_part({0.0F, 0.0F, 32.0F, 32.0F},
        {destination.x, destination.y, 32.0F, 32.0F});
    draw_part({96.0F, 0.0F, 32.0F, 32.0F},
        {destination.x + destination.width - 32.0F,
            destination.y, 32.0F, 32.0F});
    draw_part({0.0F, 96.0F, 32.0F, 32.0F},
        {destination.x, destination.y + destination.height - 32.0F,
            32.0F, 32.0F});
    draw_part({96.0F, 96.0F, 32.0F, 32.0F},
        {destination.x + destination.width - 32.0F,
            destination.y + destination.height - 32.0F, 32.0F, 32.0F});
    draw_part({40.0F, 0.0F, 48.0F, 32.0F},
        {destination.x + (destination.width - 48.0F) * 0.5F,
            destination.y, 48.0F, 32.0F});
    draw_part({40.0F, 96.0F, 48.0F, 32.0F},
        {destination.x + (destination.width - 48.0F) * 0.5F,
            destination.y + destination.height - 32.0F, 48.0F, 32.0F});
    draw_part({0.0F, 40.0F, 32.0F, 48.0F},
        {destination.x,
            destination.y + (destination.height - 48.0F) * 0.5F,
            32.0F, 48.0F});
    draw_part({96.0F, 40.0F, 32.0F, 48.0F},
        {destination.x + destination.width - 32.0F,
            destination.y + (destination.height - 48.0F) * 0.5F,
            32.0F, 48.0F});
    if (id != MaterialSpriteId::ui_warning_modal) {
        draw_part({32.0F, 32.0F, 64.0F, 64.0F},
            {destination.x + (destination.width - 64.0F) * 0.5F,
                destination.y + (destination.height - 64.0F) * 0.5F,
                64.0F, 64.0F});
    }
    ++sprite_draw_counts_[static_cast<std::size_t>(id)];
    return true;
}

bool MaterialPack::draw_horizontal_slice(MaterialSpriteId id,
    Rectangle source_within_frame, float cap_source_width,
    Rectangle destination, Color tint) const noexcept {
    if (!can_draw(id) || source_within_frame.x < 0.0F
            || source_within_frame.y < 0.0F
            || source_within_frame.width <= 0.0F
            || source_within_frame.height <= 0.0F
            || cap_source_width <= 0.0F
            || cap_source_width * 2.0F >= source_within_frame.width
            || destination.width <= 0.0F || destination.height <= 0.0F) {
        return false;
    }
    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)
            || source_within_frame.x + source_within_frame.width
                > frame->source.width
            || source_within_frame.y + source_within_frame.height
                > frame->source.height) {
        return false;
    }
    const float scale = destination.height / source_within_frame.height;
    const float cap_destination_width = cap_source_width * scale;
    if (cap_destination_width * 2.0F >= destination.width) return false;

    const std::size_t texture_index = atlas_index(frame->atlas);
    const auto draw_part = [&](Rectangle local_source,
                               Rectangle part_destination) noexcept {
        local_source.x += frame->source.x;
        local_source.y += frame->source.y;
        texture_api_.draw_material(color_textures_[texture_index],
            material_textures_[texture_index], local_source, part_destination,
            {0.0F, 0.0F}, 0.0F, tint, {});
    };
    const float middle_source_x = source_within_frame.x
        + (source_within_frame.width - 8.0F) * 0.5F;
    const Rectangle left_source{source_within_frame.x, source_within_frame.y,
        cap_source_width, source_within_frame.height};
    const Rectangle middle_source{middle_source_x, source_within_frame.y,
        8.0F, source_within_frame.height};
    const Rectangle right_source{
        source_within_frame.x + source_within_frame.width - cap_source_width,
        source_within_frame.y, cap_source_width, source_within_frame.height};
    draw_part(left_source, {destination.x, destination.y,
        cap_destination_width, destination.height});
    draw_part(middle_source, {
        destination.x + cap_destination_width, destination.y,
        destination.width - cap_destination_width * 2.0F,
        destination.height});
    draw_part(right_source, {
        destination.x + destination.width - cap_destination_width,
        destination.y, cap_destination_width, destination.height});
    ++sprite_draw_counts_[static_cast<std::size_t>(id)];
    return true;
}

bool MaterialPack::draw_region_fit(MaterialSpriteId id,
    Rectangle source_within_frame, Rectangle destination_bounds,
    Color tint) const noexcept {
    if (!can_draw(id) || source_within_frame.x < 0.0F
            || source_within_frame.y < 0.0F
            || source_within_frame.width <= 0.0F
            || source_within_frame.height <= 0.0F
            || destination_bounds.width <= 0.0F
            || destination_bounds.height <= 0.0F) {
        return false;
    }
    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)
            || source_within_frame.x + source_within_frame.width
                > frame->source.width
            || source_within_frame.y + source_within_frame.height
                > frame->source.height) {
        return false;
    }
    const float scale = std::min(
        destination_bounds.width / source_within_frame.width,
        destination_bounds.height / source_within_frame.height);
    const Rectangle destination{
        destination_bounds.x
            + (destination_bounds.width - source_within_frame.width * scale)
                * 0.5F,
        destination_bounds.y
            + (destination_bounds.height - source_within_frame.height * scale)
                * 0.5F,
        source_within_frame.width * scale,
        source_within_frame.height * scale};
    source_within_frame.x += frame->source.x;
    source_within_frame.y += frame->source.y;
    const std::size_t texture_index = atlas_index(frame->atlas);
    texture_api_.draw_material(color_textures_[texture_index],
        material_textures_[texture_index], source_within_frame, destination,
        {0.0F, 0.0F}, 0.0F, tint, {});
    ++sprite_draw_counts_[static_cast<std::size_t>(id)];
    return true;
}

std::uint64_t MaterialPack::sprite_draw_count(
    MaterialSpriteId id) const noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < sprite_draw_counts_.size()
        ? sprite_draw_counts_[index] : 0U;
}

std::uint64_t MaterialPack::direct_stretch_draw_count(
    MaterialSpriteId id) const noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < direct_stretch_draw_counts_.size()
        ? direct_stretch_draw_counts_[index] : 0U;
}

bool MaterialPack::draw_frame(MaterialAtlasId atlas, Rectangle source,
    Vector2 foot_anchor, Vector2 foot_position, bool flip_x, float scale,
    Color tint) const noexcept {
    if (!is_known_atlas(atlas) || !state_.available(atlas)
        || !valid_texture_api(texture_api_) || scale <= 0.0F
        || source.width <= 0.0F || source.height <= 0.0F
        || source.x < 0.0F || source.y < 0.0F) {
        return false;
    }
    const std::size_t index = atlas_index(atlas);
    const Texture2D& color_texture = color_textures_[index];
    const Texture2D& material_texture = material_textures_[index];
    if (!texture_api_.valid(color_texture)
        || !texture_api_.valid(material_texture)
        || source.x + source.width > static_cast<float>(color_texture.width)
        || source.y + source.height > static_cast<float>(color_texture.height)) {
        return false;
    }
    if (flip_x) {
        source.x += source.width;
        source.width = -source.width;
    }
    const Rectangle destination{foot_position.x - foot_anchor.x * scale,
        foot_position.y - foot_anchor.y * scale,
        source.width < 0.0F ? -source.width * scale : source.width * scale,
        source.height * scale};
    texture_api_.draw_material(color_texture, material_texture, source,
        destination, {0.0F, 0.0F}, 0.0F, tint, {});
    return true;
}

}  // namespace arpg::platform
