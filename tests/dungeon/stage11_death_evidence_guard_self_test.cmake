include("${CMAKE_CURRENT_LIST_DIR}/evidence_source_scan.cmake")

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef stage11_self_test_run_id)
set(stage11_self_test_scratch
    "${CMAKE_CURRENT_BINARY_DIR}/stage11-self-test-${stage11_self_test_run_id}")
file(MAKE_DIRECTORY "${stage11_self_test_scratch}")

foreach(required GUARD_SCRIPT VALID_FIXTURE VALID_FORMAL VALID_CAPTURE
        VALID_HOST_HEADER VALID_HOST_SOURCE BAD_TEST_ACCESS BAD_CAPTURE_ORDER)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Stage 11 guard self-test missing ${required}")
    endif()
endforeach()

get_filename_component(_stage_directory "${VALID_HOST_HEADER}" DIRECTORY)
set(VALID_STAGE_SOURCE "${_stage_directory}/host_validation_stage10_11.cpp")
if(NOT EXISTS "${VALID_STAGE_SOURCE}")
    message(FATAL_ERROR "Stage 11 guard self-test missing production stage source")
endif()

set_property(GLOBAL PROPERTY STAGE11_MUTATION_COUNT 0)

function(stage11_record_mutation)
    get_property(mutation_count GLOBAL PROPERTY STAGE11_MUTATION_COUNT)
    math(EXPR mutation_count "${mutation_count} + 1")
    set_property(GLOBAL PROPERTY STAGE11_MUTATION_COUNT ${mutation_count})
endfunction()

function(expect_guard_rejection name fixture host runtime expected)
    set(extra_arguments "-DSTAGE_SOURCE=${reference_stage_file}")
    if(ARGC GREATER 5)
        set(extra_arguments "-DSTAGE_SOURCE=${ARGV5}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${fixture}
            -DFORMAL_SOURCE=${VALID_FORMAL}
            -DCAPTURE_SCRIPT=${VALID_CAPTURE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${host}
            -DRUNTIME_SOURCE=${runtime}
            ${extra_arguments}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: mutated evidence was accepted")
    endif()
    string(FIND "${combined}" "${expected}" reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: wrong rejection reason; expected '${expected}', got: ${combined}")
    endif()
    stage11_record_mutation()
endfunction()

function(expect_guard_acceptance name fixture host runtime)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${fixture}
            -DFORMAL_SOURCE=${VALID_FORMAL}
            -DCAPTURE_SCRIPT=${VALID_CAPTURE}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${host}
            -DRUNTIME_SOURCE=${runtime}
            -DSTAGE_SOURCE=${reference_stage_file}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${name}: read-only evidence was rejected: ${output}\n${error}")
    endif()
endfunction()

function(expect_fixture_token_rejection name source_token expected_token)
    file(READ "${VALID_FIXTURE}" mutated_fixture_source)
    string(REPLACE "${source_token}" "" mutated_fixture_source
        "${mutated_fixture_source}")
    file(READ "${VALID_FIXTURE}" original_fixture_source)
    if(mutated_fixture_source STREQUAL original_fixture_source)
        message(FATAL_ERROR "${name}: fixture mutation anchor was not found")
    endif()
    set(mutation_file
        "${stage11_self_test_scratch}/stage11_fixture_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_fixture_source}")
    expect_guard_rejection("${name}" "${mutation_file}"
        "${reference_host_file}" "${reference_runtime_file}"
        "Fixture lacks production API: ${expected_token}")
    file(REMOVE "${mutation_file}")
endfunction()

function(expect_fixture_relocation_rejection
        name source_token relocation expected_token)
    file(READ "${VALID_FIXTURE}" mutated_fixture_source)
    string(REPLACE "${source_token}" "" mutated_fixture_source
        "${mutated_fixture_source}")
    file(READ "${VALID_FIXTURE}" original_fixture_source)
    if(mutated_fixture_source STREQUAL original_fixture_source)
        message(FATAL_ERROR "${name}: fixture relocation anchor was not found")
    endif()
    string(APPEND mutated_fixture_source "\n${relocation}\n")
    set(mutation_file
        "${stage11_self_test_scratch}/stage11_fixture_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_fixture_source}")
    expect_guard_rejection("${name}" "${mutation_file}"
        "${reference_host_file}" "${reference_runtime_file}"
        "Fixture lacks production API: ${expected_token}")
    file(REMOVE "${mutation_file}")
endfunction()

function(expect_host_replacement_rejection name old_fragment new_fragment expected)
    string(REPLACE "${old_fragment}" "${new_fragment}" mutated_source
        "${reference_host_source}")
    if(mutated_source STREQUAL reference_host_source)
        message(FATAL_ERROR "${name}: Host mutation anchor was not found")
    endif()
    set(mutation_file
        "${stage11_self_test_scratch}/stage11_host_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_source}")
    expect_guard_rejection("${name}" "${VALID_FIXTURE}" "${mutation_file}"
        "${reference_runtime_file}" "${expected}")
    file(REMOVE "${mutation_file}")
endfunction()

function(expect_runtime_replacement_rejection name old_fragment new_fragment expected)
    string(REPLACE "${old_fragment}" "${new_fragment}" mutated_source
        "${reference_runtime_source}")
    if(mutated_source STREQUAL reference_runtime_source)
        message(FATAL_ERROR "${name}: runtime mutation anchor was not found")
    endif()
    set(mutation_file
        "${stage11_self_test_scratch}/stage11_runtime_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_source}")
    expect_guard_rejection("${name}" "${VALID_FIXTURE}"
        "${reference_host_file}" "${mutation_file}" "${expected}")
    file(REMOVE "${mutation_file}")
endfunction()

function(expect_capture_replacement_rejection name old_fragment new_fragment)
    file(READ "${VALID_CAPTURE}" mutated_capture_source)
    string(REPLACE "${old_fragment}" "${new_fragment}" mutated_capture_source
        "${mutated_capture_source}")
    file(READ "${VALID_CAPTURE}" original_capture_source)
    if(mutated_capture_source STREQUAL original_capture_source)
        message(FATAL_ERROR "${name}: capture mutation anchor was not found")
    endif()
    set(mutation_file
        "${stage11_self_test_scratch}/stage11_capture_${name}.ps1")
    file(WRITE "${mutation_file}" "${mutated_capture_source}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -DFIXTURE_SOURCE=${VALID_FIXTURE}
            -DFORMAL_SOURCE=${VALID_FORMAL}
            -DCAPTURE_SCRIPT=${mutation_file}
            -DHOST_HEADER=${VALID_HOST_HEADER}
            -DHOST_SOURCE=${reference_host_file}
            -DRUNTIME_SOURCE=${reference_runtime_file}
            -DSTAGE_SOURCE=${reference_stage_file}
            -P ${GUARD_SCRIPT}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${name}: weakened capture baseline was accepted")
    endif()
    string(FIND "${combined}" "Capture validator lacks" reason_index)
    if(reason_index EQUAL -1)
        message(FATAL_ERROR
            "${name}: wrong capture rejection reason: ${combined}")
    endif()
    stage11_record_mutation()
    file(REMOVE "${mutation_file}")
endfunction()

set(expected_should_continue_death [=[
bool HostValidationRuntime::should_continue_death(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    const bool pending = snapshot.death.has_value()
        && snapshot.death->can_continue && !snapshot.death->saving;
    const bool stage10_validation_continue =
        impl_->config->stage10_validation
            == Stage10ValidationScenario::player_death;
    const auto scenario = impl_->config->stage11_validation;
    const bool stage11_validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    return pending && (stage10_validation_continue
        || (stage11_validation_continue
            && !impl_->states.stage11.continue_requested));
}
]=])
set(expected_observe_death_continue_result [=[
void HostValidationRuntime::observe_death_continue_result(
    dungeon::RequestResult result) noexcept {
    const auto scenario = impl_->config->stage11_validation;
    const bool validation_continue =
        scenario == Stage11ValidationScenario::deep_continue
        || scenario == Stage11ValidationScenario::floor_one_continue;
    if (validation_continue
            && result != dungeon::RequestResult::rejected) {
        impl_->states.stage11.continue_requested = true;
    }
}
]=])
set(expected_fixed_step_movement [=[
combat::MovementInput HostValidationRuntime::fixed_step_movement(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    combat::MovementInput production_input) noexcept {
    if (impl_->config->stage11_validation
            != Stage11ValidationScenario::none) {
        return host_validation::stage11_validation_input(
            session, snapshot, *impl_->config, impl_->states.stage11);
    }
    if (impl_->config->stage10_validation
            != Stage10ValidationScenario::none) {
        return host_validation::stage10_validation_input(
            session, snapshot, *impl_->config, impl_->states.stage10);
    }
    return production_input;
}
]=])
set(expected_fixed_step_target_reached [=[
bool HostValidationRuntime::fixed_step_target_reached(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    return host_validation::stage10_validation_reached(
        snapshot, *impl_->config, impl_->states.stage10)
        || host_validation::stage11_validation_reached(
            snapshot, *impl_->config, impl_->states.stage11);
}
]=])
set(expected_observe_presented_frame [=[
PresentationDecision HostValidationRuntime::observe_presented_frame(
    const dungeon::DungeonSnapshot& snapshot,
    const PauseMenuState& pause_menu,
    bool pause_cjk_ready) noexcept {
    const bool stage11_target_visible = host_validation::stage11_validation_reached(
        snapshot, *impl_->config, impl_->states.stage11);
    if (stage11_target_visible) {
        ++impl_->states.stage11.target_presented_frames;
    } else {
        impl_->states.stage11.target_presented_frames = 0U;
    }
    const bool stage11_reached = stage11_target_visible
        && impl_->states.stage11.target_presented_frames >= 4U;
    const bool stage10_reached = false;
    const bool stage11b_reached = false;
    const bool stage11c_reached = false;
    const bool stage11d_reached = false;
    const bool stage17_reached = false;
    PresentationDecision decision{};
    decision.validation_complete = stage10_reached || stage11_reached
        || stage11b_reached || stage11c_reached
        || stage11d_reached || stage17_reached;
    static_cast<void>(pause_menu);
    static_cast<void>(pause_cjk_ready);
    return decision;
}
]=])
string(CONCAT reference_runtime_source
    "namespace arpg::platform {\n"
    "${expected_should_continue_death}\n"
    "${expected_observe_death_continue_result}\n"
    "${expected_fixed_step_movement}\n"
    "${expected_fixed_step_target_reached}\n"
    "${expected_observe_presented_frame}\n"
    "}  // namespace arpg::platform\n")
set(reference_runtime_file
    "${stage11_self_test_scratch}/stage11_reference_runtime.cpp")
file(WRITE "${reference_runtime_file}" "${reference_runtime_source}")

# The mutation inventory exercises the guard itself, so use a compact Stage
# algorithm fixture here.  The direct production guard still reads and checks
# the real host_validation_stage10_11.cpp.
set(reference_stage_source [=[
combat::MovementInput stage11_validation_input(
    dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    Stage11ValidationState& state) noexcept {
    static_cast<void>(config);
    const bool drive_to_depth = true;
    if (drive_to_depth && snapshot.exits_unlocked && snapshot.has_hole
            && snapshot.phase == dungeon::RoomPhase::combat) {
        settle_grid_route_movement(state.sweep_grid,
            snapshot.combat->player.position);
        const auto movement = grid_route_movement(
            snapshot.combat->player.position, kHoleCenter, state.sweep_grid);
        if (can_prompt_descent(snapshot, snapshot.combat->player.position)) {
            static_cast<void>(session.request_descent(true));
        }
        return movement;
    }
    return stage10_validation_input(
        session, snapshot, config, state.combat_driver);
}

bool stage11_validation_reached(
    const dungeon::DungeonSnapshot& snapshot,
    const RaylibHostConfig& config,
    const Stage11ValidationState& state) noexcept {
    static_cast<void>(snapshot);
    const bool deep = config.stage11_validation
        == Stage11ValidationScenario::deep_continue;
    return deep && state.continue_requested && state.saw_depth_two;
}
]=])
set(reference_stage_file
    "${stage11_self_test_scratch}/stage11_reference_stage.cpp")
file(WRITE "${reference_stage_file}" "${reference_stage_source}")

set(reference_continue_decision [=[
if (validation_runtime->should_continue_death(current)) {
        death_gate.continue_death = true;
    }
]=])
set(reference_death_gate [=[
if (death_gate.continue_death) {
        death_continue_result = runtime.request_death_continue();
        if (death_continue_result != dungeon::RequestResult::rejected) {
            previous = current;
            session->snapshot(current);
        }
        validation_runtime->observe_death_continue_result(
            death_continue_result);
    }
]=])
set(reference_host_source [=[
bool present_frame_and_maybe_capture(const char* path) noexcept {
    EndDrawing();
    if (path == nullptr) return true;
    Image image = LoadImageFromScreen();
    if (image.data == nullptr) return false;
    const bool exported = ExportImage(image, path);
    UnloadImage(image);
    return exported;
}

HostExitCode run_raylib_host() noexcept {
    const auto death_key = settings::StableKey::e;
    const bool death_saving = current.death.has_value()
        && current.death->saving;
    const bool death_pending = current.death.has_value()
        && current.death->can_continue;
    DeathInputGate death_gate = host_death_input_gate(
        death_saving, death_pending, frame_input.keys, physical_keys);
    if (validation_runtime->should_continue_death(current)) {
        death_gate.continue_death = true;
    }
    dungeon::RequestResult death_continue_result =
        dungeon::RequestResult::rejected;
    if (death_gate.continue_death) {
        death_continue_result = runtime.request_death_continue();
        if (death_continue_result != dungeon::RequestResult::rejected) {
            previous = current;
            session->snapshot(current);
        }
        validation_runtime->observe_death_continue_result(
            death_continue_result);
    }
    if (!death_gate.forward_gameplay && window_close_requested) {
        begin_clean_exit();
    }

    core::FixedStepFrame frame{};
    combat::MovementInput movement{};
    for (std::uint32_t step = 0; step < frame.steps; ++step) {
        previous = current;
        const bool step_death = current.death.has_value();
        combat::MovementInput step_movement{};
        if (!step_death) {
            step_movement = validation_runtime->fixed_step_movement(
                *session, current, movement);
        }
        runtime.fixed_tick(step_movement, loot_pickup_policy);
        validation_runtime->observe_fixed_tick();
        session->snapshot(current);
        validation_runtime->observe_snapshot(current);
        validation_runtime->observe_post_fixed_tick(
            current, &session->item_state());
        drain_events(*session);
        if (validation_runtime->fixed_step_target_reached(current)) {
            break;
        }
    }
    const PresentationDecision decision =
        validation_runtime->observe_presented_frame(
            current, pause_menu, pause_cjk_ready);
    static_cast<void>(decision);
    return HostExitCode::success;
}
]=])
set(reference_host_file
    "${stage11_self_test_scratch}/stage11_reference_host.cpp")
file(WRITE "${reference_host_file}" "${reference_host_source}")

expect_guard_acceptance(reference_owner_contract "${VALID_FIXTURE}"
    "${reference_host_file}" "${reference_runtime_file}")

expect_fixture_token_rejection(missing_storage "SaveCommitStorage"
    "SaveCommitStorage")
expect_fixture_token_rejection(missing_submit "worker_->submit("
    "submit[ \\t\\r\\n]*\\(")
expect_fixture_token_rejection(missing_completion "try_take_completion"
    "try_take_completion")
expect_fixture_token_rejection(missing_envelope "inspect_checkpoint_latest_envelope"
    "inspect_checkpoint_latest_envelope")
expect_fixture_token_rejection(missing_loaded_checkpoint "loaded_checkpoint"
    "loaded_checkpoint")
expect_fixture_token_rejection(missing_restore "restore_room_progress_checkpoint"
    "restore_room_progress_checkpoint")
expect_fixture_token_rejection(missing_release "release_loaded_checkpoints"
    "release_loaded_checkpoints")
expect_fixture_token_rejection(missing_worker_stop "stop_and_join"
    "stop_and_join")
expect_fixture_token_rejection(missing_room_compare
    "same_room_progress_checkpoint" "same_room_progress_checkpoint")
expect_fixture_relocation_rejection(comment_only_envelope
    "inspect_checkpoint_latest_envelope"
    "// inspect_checkpoint_latest_envelope" "inspect_checkpoint_latest_envelope")
expect_fixture_relocation_rejection(inactive_envelope
    "inspect_checkpoint_latest_envelope"
    "#if 0\nvoid inactive_fixture_decoy() { inspect_checkpoint_latest_envelope; }\n#endif"
    "inspect_checkpoint_latest_envelope")

# Preserve all legacy injection and capture-order coverage against the new
# owner reference rather than borrowing the still-RED production Host.
expect_guard_rejection(test_access "${BAD_TEST_ACCESS}" "${reference_host_file}"
    "${reference_runtime_file}"
    "Forbidden Stage 11 evidence injection: DungeonSessionTestAccess")
set(public_mutations
    "checkpoint.next_state.death = fabricated_death"
    "checkpoint_ptr->death = fabricated_death"
    "death_snapshot.emplace(fabricated_death)"
    "death_snapshot = fabricated_death"
    "death_checkpoint.emplace(fabricated_death)"
    "death_checkpoint = fabricated_death"
    "auto death = make_death_checkpoint(fabricated_combat, room, target)"
    "checkpoint.death.lifecycle = pending_continue"
    "checkpoint.death.target_room.seed = 42"
    "checkpoint_ptr->death.final_damage = 0"
    "checkpoint.death.recent_damage[0] += 1"
    "checkpoint.death.final_damage -= 1"
    "checkpoint_ptr->death.target_room.seed |= 1")
set(public_index 0)
foreach(public_mutation IN LISTS public_mutations)
    math(EXPR public_index "${public_index} + 1")
    set(mutation_file
        "${stage11_self_test_scratch}/stage11_bad_public_death_${public_index}.txt")
    file(WRITE "${mutation_file}" "${public_mutation}\n")
    expect_guard_rejection("public_death_${public_index}" "${mutation_file}"
        "${reference_host_file}" "${reference_runtime_file}"
        "Forbidden Stage 11 public death injection")
    file(REMOVE "${mutation_file}")
endforeach()

file(READ "${VALID_FIXTURE}" valid_fixture_source)
set(read_only_file
    "${stage11_self_test_scratch}/stage11_read_only_death_comparisons.txt")
file(WRITE "${read_only_file}" "${valid_fixture_source}\n"
    "// checkpoint.death == expected_death\n"
    "// checkpoint.death != other_death\n"
    "// checkpoint.death.lifecycle == pending_continue\n"
    "// checkpoint.death.target_room.seed != expected_seed\n")
expect_guard_acceptance(read_only_death_comparisons "${read_only_file}"
    "${reference_host_file}" "${reference_runtime_file}")
file(REMOVE "${read_only_file}")

expect_guard_rejection(capture_order "${VALID_FIXTURE}" "${BAD_CAPTURE_ORDER}"
    "${reference_runtime_file}" "Capture must occur once after EndDrawing")

expect_host_replacement_rejection(validation_continue_bypass
    "DeathInputGate death_gate = host_death_input_gate("
    "static_cast<void>(runtime.request_death_continue());\n    DeathInputGate death_gate = host_death_input_gate("
    "Formal validation continue must use the single death input gate")

foreach(gate_condition IN ITEMS
        "death_gate.continue_death || validation_continue"
        "validation_continue")
    string(MAKE_C_IDENTIFIER "${gate_condition}" mutation_suffix)
    expect_host_replacement_rejection("death_gate_${mutation_suffix}"
        "if (death_gate.continue_death) {" "if (${gate_condition}) {"
        "Formal death continue condition must be exactly death_gate.continue_death")
endforeach()

expect_host_replacement_rejection(host_hidden_continue_write
    "death_gate.continue_death = true;"
    "validation_runtime->stage11.continue_requested = true;\n        death_gate.continue_death = true;"
    "must not access continue_requested")

# Runtime owner mutations: exact decision semantics, faulted acceptance, no
# submission, scenario priority, and Stage10-before-Stage11 reached ordering.
expect_runtime_replacement_rejection(missing_should_continue
    "${expected_should_continue_death}" ""
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
string(REGEX REPLACE "[ \t\r\n]+" "" compact_should_continue
    "${expected_should_continue_death}")
expect_runtime_replacement_rejection(should_comment_decoy
    "${expected_should_continue_death}"
    "/* ${expected_should_continue_death} */"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
expect_runtime_replacement_rejection(should_string_decoy
    "${expected_should_continue_death}"
    "constexpr const char* decoy = \"${compact_should_continue}\";"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
expect_runtime_replacement_rejection(should_raw_decoy
    "${expected_should_continue_death}"
    "constexpr const char* decoy = R\"guard(${expected_should_continue_death})guard\";"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
expect_runtime_replacement_rejection(should_inactive_decoy
    "${expected_should_continue_death}"
    "#if 0\n${expected_should_continue_death}\n#endif"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
expect_runtime_replacement_rejection(should_lambda_override
    "${expected_should_continue_death}"
    "auto forged_override = [] {\n${expected_should_continue_death}\n};"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
expect_runtime_replacement_rejection(should_dead_override
    "${expected_should_continue_death}"
    "if (false) {\n${expected_should_continue_death}\n}"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
string(REPLACE "${expected_should_continue_death}" ""
    should_cross_scope_source "${reference_runtime_source}")
string(APPEND should_cross_scope_source
    "\n${expected_should_continue_death}\n")
set(should_cross_scope_file
    "${stage11_self_test_scratch}/stage11_runtime_should_cross_scope.cpp")
file(WRITE "${should_cross_scope_file}" "${should_cross_scope_source}")
expect_guard_rejection(should_cross_scope_override "${VALID_FIXTURE}"
    "${reference_host_file}" "${should_cross_scope_file}"
    "Stage 11 runtime should_continue_death owner contract is missing or altered")
file(REMOVE "${should_cross_scope_file}")
expect_runtime_replacement_rejection(observer_accepts_only_accepted
    "result != dungeon::RequestResult::rejected"
    "result == dungeon::RequestResult::accepted"
    "Stage 11 runtime death-result observer contract is missing or altered")
expect_runtime_replacement_rejection(observer_submits_request
    "impl_->states.stage11.continue_requested = true;"
    "runtime.request_death_continue();\n        impl_->states.stage11.continue_requested = true;"
    "Stage 11 runtime death-result observer contract is missing or altered")
expect_runtime_replacement_rejection(stage11_zero_falls_through
    "return host_validation::stage11_validation_input(\n            session, snapshot, *impl_->config, impl_->states.stage11);"
    "production_input = host_validation::stage11_validation_input(\n            session, snapshot, *impl_->config, impl_->states.stage11);"
    "Stage 11 runtime fixed-step movement owner contract is missing or altered")
set(swapped_target_reached [=[
bool HostValidationRuntime::fixed_step_target_reached(
    const dungeon::DungeonSnapshot& snapshot) const noexcept {
    return host_validation::stage11_validation_reached(
        snapshot, *impl_->config, impl_->states.stage11)
        || host_validation::stage10_validation_reached(
            snapshot, *impl_->config, impl_->states.stage10);
}
]=])
expect_runtime_replacement_rejection(reached_order_swapped
    "${expected_fixed_step_target_reached}" "${swapped_target_reached}"
    "Stage 11 runtime fixed-step reached owner contract is missing or altered")
expect_runtime_replacement_rejection(presentation_increment_missing
    "++impl_->states.stage11.target_presented_frames;"
    "static_cast<void>(impl_->states.stage11.target_presented_frames);"
    "T7C Stage11 presentation branch contract is missing or altered")
expect_runtime_replacement_rejection(presentation_reset_missing
    "impl_->states.stage11.target_presented_frames = 0U;"
    "static_cast<void>(impl_->states.stage11.target_presented_frames);"
    "T7C Stage11 presentation branch contract is missing or altered")
expect_runtime_replacement_rejection(presentation_branch_inverted
    "if (stage11_target_visible) {"
    "if (!stage11_target_visible) {"
    "T7C Stage11 presentation branch contract is missing or altered")
expect_runtime_replacement_rejection(presentation_threshold_changed
    "impl_->states.stage11.target_presented_frames >= 4U"
    "impl_->states.stage11.target_presented_frames >= 3U"
    "T7C Stage11 presentation branch contract is missing or altered")
expect_runtime_replacement_rejection(presentation_completion_discarded
    "stage10_reached || stage11_reached"
    "stage10_reached"
    "T7C Stage11 decision completion contract is missing or altered")
expect_runtime_replacement_rejection(presentation_default_return
    "return decision;"
    "return {};"
    "T7C Stage11 decision completion contract is missing or altered")

set(stage11_presentation_branch [=[
    const bool stage11_target_visible = host_validation::stage11_validation_reached(
        snapshot, *impl_->config, impl_->states.stage11);
    if (stage11_target_visible) {
        ++impl_->states.stage11.target_presented_frames;
    } else {
        impl_->states.stage11.target_presented_frames = 0U;
    }
    const bool stage11_reached = stage11_target_visible
        && impl_->states.stage11.target_presented_frames >= 4U;
]=])
string(REGEX REPLACE "[ \t\r\n]+" " " stage11_presentation_branch_compact
    "${stage11_presentation_branch}")
foreach(stage11_decoy_kind IN ITEMS comment string raw inactive lambda dead)
    if(stage11_decoy_kind STREQUAL "comment")
        set(stage11_presentation_decoy
            "    /* ${stage11_presentation_branch} */\n")
    elseif(stage11_decoy_kind STREQUAL "string")
        set(stage11_presentation_decoy
            "    constexpr const char* stage11_decoy = \"${stage11_presentation_branch_compact}\";\n")
    elseif(stage11_decoy_kind STREQUAL "raw")
        set(stage11_presentation_decoy
            "    constexpr const char* stage11_decoy = R\"guard(${stage11_presentation_branch})guard\";\n")
    elseif(stage11_decoy_kind STREQUAL "inactive")
        set(stage11_presentation_decoy
            "#if 0\n${stage11_presentation_branch}#endif\n")
    elseif(stage11_decoy_kind STREQUAL "lambda")
        set(stage11_presentation_decoy
            "    const auto stage11_decoy = [&] {\n${stage11_presentation_branch}    };\n")
    else()
        set(stage11_presentation_decoy
            "    if (false) {\n${stage11_presentation_branch}    }\n")
    endif()
    expect_runtime_replacement_rejection(
        "presentation_${stage11_decoy_kind}_decoy"
        "${stage11_presentation_branch}" "${stage11_presentation_decoy}"
        "T7C Stage11 presentation branch contract is missing or altered")
endforeach()

string(REPLACE "${stage11_presentation_branch}" ""
    stage11_presentation_cross_scope_source "${reference_runtime_source}")
string(APPEND stage11_presentation_cross_scope_source
    "\nvoid stage11_presentation_cross_scope_decoy() {\n${stage11_presentation_branch}}\n")
set(stage11_presentation_cross_scope_file
    "${stage11_self_test_scratch}/stage11_runtime_presentation_cross_scope.cpp")
file(WRITE "${stage11_presentation_cross_scope_file}"
    "${stage11_presentation_cross_scope_source}")
expect_guard_rejection(presentation_cross_scope_decoy "${VALID_FIXTURE}"
    "${reference_host_file}" "${stage11_presentation_cross_scope_file}"
    "T7C Stage11 presentation branch contract is missing or altered")
file(REMOVE "${stage11_presentation_cross_scope_file}")

# A syntactically convincing facade decision outside the direct live scope must
# never replace the real Host decision.
set(decision_comment
    "// if (validation_runtime->should_continue_death(current)) { death_gate.continue_death = true; }")
set(decision_string
    "const char* decision_decoy = \"if (validation_runtime->should_continue_death(current)) { death_gate.continue_death = true; }\";")
set(decision_raw
    "const char* decision_decoy = R\"guard(if (validation_runtime->should_continue_death(current)) { death_gate.continue_death = true; })guard\";")
set(decision_inactive "#if 0\n${reference_continue_decision}\n#endif")
set(decision_lambda "auto decision_decoy = [&] {\n${reference_continue_decision}\n    };")
set(decision_dead "if (false) {\n${reference_continue_decision}\n    }")
foreach(decoy_name IN ITEMS comment string raw inactive lambda dead)
    expect_host_replacement_rejection("decision_${decoy_name}_decoy"
        "${reference_continue_decision}" "${decision_${decoy_name}}"
        "Stage 11 Host continue decision must use the runtime facade only")
endforeach()
string(REPLACE "${reference_continue_decision}" ""
    decision_cross_scope_source "${reference_host_source}")
string(REPLACE "HostExitCode run_raylib_host() noexcept {"
    "void forged_decision() {\n${reference_continue_decision}\n}\n\nHostExitCode run_raylib_host() noexcept {"
    decision_cross_scope_source "${decision_cross_scope_source}")
set(decision_cross_scope_file
    "${stage11_self_test_scratch}/stage11_decision_cross_scope.cpp")
file(WRITE "${decision_cross_scope_file}" "${decision_cross_scope_source}")
expect_guard_rejection(decision_cross_scope_decoy "${VALID_FIXTURE}"
    "${decision_cross_scope_file}" "${reference_runtime_file}"
    "continue decision must use the runtime facade")
file(REMOVE "${decision_cross_scope_file}")

# The observer must consume the real result after the non-rejected snapshot
# refresh, inside the one real death gate.
expect_host_replacement_rejection(missing_death_observer
    "        validation_runtime->observe_death_continue_result(\n            death_continue_result);\n"
    "" "death observer must follow the real request")

string(REPLACE
    "        validation_runtime->observe_death_continue_result(\n            death_continue_result);\n"
    "" observer_before_request_source "${reference_host_source}")
string(REPLACE
    "        death_continue_result = runtime.request_death_continue();"
    "        validation_runtime->observe_death_continue_result(\n            death_continue_result);\n        death_continue_result = runtime.request_death_continue();"
    observer_before_request_source "${observer_before_request_source}")
set(observer_before_request_file
    "${stage11_self_test_scratch}/stage11_observer_before_request.cpp")
file(WRITE "${observer_before_request_file}" "${observer_before_request_source}")
expect_guard_rejection(observer_before_request "${VALID_FIXTURE}"
    "${observer_before_request_file}" "${reference_runtime_file}"
    "death observer must follow the real request")
file(REMOVE "${observer_before_request_file}")

string(REPLACE
    "        validation_runtime->observe_death_continue_result(\n            death_continue_result);\n"
    "" observer_before_snapshot_source "${reference_host_source}")
string(REPLACE "            previous = current;"
    "            validation_runtime->observe_death_continue_result(\n                death_continue_result);\n            previous = current;"
    observer_before_snapshot_source "${observer_before_snapshot_source}")
set(observer_before_snapshot_file
    "${stage11_self_test_scratch}/stage11_observer_before_snapshot.cpp")
file(WRITE "${observer_before_snapshot_file}" "${observer_before_snapshot_source}")
expect_guard_rejection(observer_before_snapshot "${VALID_FIXTURE}"
    "${observer_before_snapshot_file}" "${reference_runtime_file}"
    "death observer must follow the real request")
file(REMOVE "${observer_before_snapshot_file}")

expect_host_replacement_rejection(observer_fabricated_result
    "validation_runtime->observe_death_continue_result(\n            death_continue_result);"
    "validation_runtime->observe_death_continue_result(\n            dungeon::RequestResult::accepted);"
    "death observer must follow the real request")
expect_host_replacement_rejection(observer_duplicate
    "validation_runtime->observe_death_continue_result(\n            death_continue_result);"
    "validation_runtime->observe_death_continue_result(\n            death_continue_result);\n        validation_runtime->observe_death_continue_result(\n            death_continue_result);"
    "death observer must follow the real request")
expect_host_replacement_rejection(discarded_fixed_step_movement
    "step_movement = validation_runtime->fixed_step_movement(\n                *session, current, movement);"
    "static_cast<void>(validation_runtime->fixed_step_movement(\n                *session, current, movement));"
    "movement result must feed step_movement")
expect_host_replacement_rejection(discarded_target_then_break
    "if (validation_runtime->fixed_step_target_reached(current)) {\n            break;\n        }"
    "static_cast<void>(validation_runtime->fixed_step_target_reached(current));\n        break;"
    "reached result must directly guard break")

function(expect_stage_route_rejection name token inject_brace_noise decoy_kind)
    set(stage_source "${reference_stage_source}")
    evidence_find_cpp_function_bounds("${stage_source}"
        "combat::MovementInput stage11_validation_input("
        stage11_begin stage11_open stage11_end)
    math(EXPR stage11_input_length "${stage11_end} - ${stage11_begin} + 1")
    string(SUBSTRING "${stage_source}" ${stage11_begin} ${stage11_input_length}
        stage11_input)
    string(REPLACE "${token}" "" mutated_input "${stage11_input}")
    if(mutated_input STREQUAL stage11_input)
        message(FATAL_ERROR "${name}: mutation token was not found")
    endif()
    set(injected_noise "")
    if(inject_brace_noise)
        string(APPEND injected_noise
            "\n    // { line-comment brace\n    /* { block-comment brace } */\n    const char* evidence_brace_string = \"{\\\"}\";\n    const char evidence_brace_character = '{';\n")
    endif()
    string(ASCII 92 splice_backslash)
    if(decoy_kind STREQUAL "continued_line")
        string(APPEND injected_noise
            "\n    // decoy follows ${splice_backslash}\n    ${token};\n")
    elseif(decoy_kind STREQUAL "spliced_slashes")
        string(APPEND injected_noise
            "\n    /${splice_backslash}\n/ ${token};\n")
    elseif(NOT decoy_kind STREQUAL "none")
        message(FATAL_ERROR "${name}: unknown decoy kind ${decoy_kind}")
    endif()
    if(NOT injected_noise STREQUAL "")
        math(EXPR stage11_open_after "${stage11_open} - ${stage11_begin} + 1")
        string(SUBSTRING "${mutated_input}" 0 ${stage11_open_after}
            input_prefix)
        string(SUBSTRING "${mutated_input}" ${stage11_open_after} -1
            input_suffix)
        set(mutated_input "${input_prefix}${injected_noise}${input_suffix}")
    endif()
    string(REPLACE "${stage11_input}" "${mutated_input}" mutated_source
        "${stage_source}")
    set(mutation_file "${stage11_self_test_scratch}/stage11_${name}.cpp")
    file(WRITE "${mutation_file}" "${mutated_source}")
    expect_guard_rejection("${name}" "${VALID_FIXTURE}"
        "${reference_host_file}" "${reference_runtime_file}"
        "Stage 11 validation input lacks production route" "${mutation_file}")
    file(REMOVE "${mutation_file}")
endfunction()

expect_stage_route_rejection(missing_descent "session.request_descent(true)" FALSE none)
expect_stage_route_rejection(missing_survival_driver
    "stage10_validation_input(" FALSE none)
expect_stage_route_rejection(missing_descent_with_brace_noise
    "session.request_descent(true)" TRUE none)
expect_stage_route_rejection(missing_survival_driver_with_brace_noise
    "stage10_validation_input(" TRUE none)
expect_stage_route_rejection(missing_descent_with_continued_line_comment
    "session.request_descent(true)" FALSE continued_line)
expect_stage_route_rejection(missing_survival_with_continued_line_comment
    "stage10_validation_input(" FALSE continued_line)
expect_stage_route_rejection(missing_descent_with_spliced_slashes
    "session.request_descent(true)" FALSE spliced_slashes)
expect_stage_route_rejection(missing_survival_with_spliced_slashes
    "stage10_validation_input(" FALSE spliced_slashes)

set(relocated_early_descent_source "${reference_stage_source}")
string(REPLACE
    "            static_cast<void>(session.request_descent(true));\n"
    ""
    relocated_early_descent_source "${relocated_early_descent_source}")
string(REPLACE
    "    return stage10_validation_input("
    "    session.request_descent(true);\n    return stage10_validation_input("
    relocated_early_descent_source "${relocated_early_descent_source}")
if(relocated_early_descent_source STREQUAL reference_stage_source)
    message(FATAL_ERROR
        "relocated_early_descent: mutation anchor was not found")
endif()
set(relocated_early_descent_file
    "${stage11_self_test_scratch}/stage11_relocated_early_descent.cpp")
file(WRITE "${relocated_early_descent_file}"
    "${relocated_early_descent_source}")
expect_guard_rejection(relocated_early_descent "${VALID_FIXTURE}"
    "${reference_host_file}" "${reference_runtime_file}"
    "Stage 11 early hole route lacks production descent"
    "${relocated_early_descent_file}")
file(REMOVE "${relocated_early_descent_file}")

expect_capture_replacement_rejection(capture_dark_floor
    "\$panelDark -lt 500" "\$panelDark -lt 0")
expect_capture_replacement_rejection(capture_accent_floor
    "\$panelAccent -lt 15" "\$panelAccent -lt 0")
expect_capture_replacement_rejection(capture_authored_floor
    "\$panelAuthored -lt 3000" "\$panelAuthored -lt 0")
expect_capture_replacement_rejection(capture_panel_left
    "\$panelLeft = 152" "\$panelLeft = 120")
expect_capture_replacement_rejection(capture_panel_top
    "\$panelTop = 80" "\$panelTop = 48")
expect_capture_replacement_rejection(capture_panel_right
    "\$panelRightExclusive = 1128" "\$panelRightExclusive = 1160")
expect_capture_replacement_rejection(capture_panel_bottom
    "\$panelBottomExclusive = 644" "\$panelBottomExclusive = 672")

get_property(final_mutation_count GLOBAL PROPERTY STAGE11_MUTATION_COUNT)
if(NOT final_mutation_count EQUAL 85)
    message(FATAL_ERROR
        "Stage 11 guard mutation inventory drifted: expected 85, got ${final_mutation_count}")
endif()
file(REMOVE "${reference_host_file}" "${reference_runtime_file}"
    "${reference_stage_file}")
file(REMOVE_RECURSE "${stage11_self_test_scratch}")
message(STATUS
    "Stage 11 guard mutation self-test passed (${final_mutation_count} mutations)")
