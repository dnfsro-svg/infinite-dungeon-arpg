#pragma once

#include "material_manifest.hpp"

#include "dungeon/dungeon_types.hpp"

#include <cstddef>
#include <cstdint>

namespace arpg::platform {

using MaterialAtlasMask = std::uint64_t;
static_assert(static_cast<std::size_t>(MaterialAtlasId::count) <= 64U);

struct MaterialResidencyRequest final {
    MaterialAtlasMask atlases{};
    void require(MaterialAtlasId id) noexcept;
    [[nodiscard]] bool contains(MaterialAtlasId id) const noexcept;
    friend bool operator==(MaterialResidencyRequest lhs,
        MaterialResidencyRequest rhs) noexcept {
        return lhs.atlases == rhs.atlases;
    }
    friend bool operator!=(MaterialResidencyRequest lhs,
        MaterialResidencyRequest rhs) noexcept { return !(lhs == rhs); }
};

[[nodiscard]] MaterialResidencyRequest
base_material_residency_request() noexcept;
[[nodiscard]] MaterialResidencyRequest make_material_residency_request(
    const dungeon::DungeonSnapshot& snapshot) noexcept;
[[nodiscard]] std::size_t material_residency_bytes(
    const MaterialManifestDefinition& manifest,
    MaterialResidencyRequest request) noexcept;

}  // namespace arpg::platform
