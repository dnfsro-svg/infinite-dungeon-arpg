#pragma once

#include "checkpoint/dungeon_run_state.hpp"

namespace arpg::dungeon::checkpoint {

using ExitDirection = ::arpg::checkpoint::ExitDirection;
using EntrySide = ::arpg::checkpoint::EntrySide;
using DungeonElement = ::arpg::checkpoint::DungeonElement;
using TransitionKind = ::arpg::checkpoint::TransitionKind;
using RoomDescriptor = ::arpg::checkpoint::RoomDescriptor;
using DeathLifecycle = ::arpg::checkpoint::DeathLifecycle;
using DeathSourceKind = ::arpg::checkpoint::DeathSourceKind;
using DeathDamageType = ::arpg::checkpoint::DeathDamageType;
using DeathCheckpoint = ::arpg::checkpoint::DeathCheckpoint;
using AbyssCheckpoint = ::arpg::checkpoint::AbyssCheckpoint;
using LastAbyssResolution = ::arpg::checkpoint::LastAbyssResolution;
using DungeonRunState = ::arpg::checkpoint::DungeonRunState;

inline constexpr std::uint8_t kDeathCheckpointDataVersion =
    ::arpg::checkpoint::kDeathCheckpointDataVersion;

using ::arpg::checkpoint::valid_abyss_door_origin;
using ::arpg::checkpoint::valid_death_checkpoint_structural;

}  // namespace arpg::dungeon::checkpoint
