# Task 5 foundation review

- Baseline: `c6ecd4f`
- Scope: `DeterministicRng`, `PlayerDamageHistory`, `EffectSet`, `RoomCombatCheckpoint`, and `CombatWorld` capture/restore only.
- Excluded: `RoomProgressCheckpoint`, V9 codec, save worker/storage, and runtime transaction wiring.
- Method: read-only source/test review. Per controller instruction, no build or test process was started.

## Strengths

- `DeterministicRng::import_state()` rejects the forbidden all-zero state before assignment, so rejection is non-mutating; the tests also compare the exact next sample after restore.
- `PlayerDamageHistory::restore_checkpoint()` validates before assignment and checks per-damage-type accumulation overflow without allocating.
- The checkpoint is non-copyable/non-movable and is consumed by reference; the tests allocate the full object on the heap.
- Monster records are emitted in ordinal order and active/persistent residents share one field-wise capture format.
- During this review the implementer fixed three early blockers: active resident write-back no longer overwrites restored monster state, active-skill capture now emits a non-skill player state, and the two huge array clears no longer use whole-array aggregate temporaries.

## Findings

### P0

1. **Restore can return `false` after partially publishing checkpoint authority.**
   - Files: `src/combat/room_combat_checkpoint.cpp:474-559`, `src/combat/room_monster_field.cpp:392-404`
   - `tick_`, RNG, player, attack, skill cooldowns, abyss state, and damage history are assigned before `required_residents()`, `synchronize_active_region()`, legacy monster lookup, and obstacle lookup have finished. Any later failure returns `false` with a mixed old/new world. A structured malformed legacy checkpoint with a valid but absent monster ordinal reaches this path directly; a residency failure has the same shape.
   - This violates the Task 5 requirement to validate/rebuild before applying player, monster, or obstacle state, and makes the boolean failure contract unsafe.
   - Fix: perform every fallible lookup, capacity check, effect validation, and required-resident calculation before the first live assignment. Prefer restoring a disposable/preallocated candidate world/field and publishing it only after synchronization succeeds. The final commit section should contain no reachable `return false`.

### P1

1. **Death, history, and player-death authority are not cross-validated.**
   - Files: `src/combat/room_combat_checkpoint.cpp:139-173`, `src/combat/room_combat_checkpoint.cpp:370-472`, `src/combat/player_damage_history.cpp:64-86`
   - An initialized history with `active_tick > checkpoint.tick` is accepted. `has_death_snapshot` is not tied to `player.hp == 0`; the death defense snapshot is not required to equal the restored player defense; `recent_damage` is not required to equal history totals; and the damage source is only enum-range checked rather than canonicalized. A checkpoint with `player.hp == 0` and no death snapshot is also accepted.
   - Fix: add an overflow-safe room-level history/tick validator, require exact death/player/history equality, and require the stored source to equal its canonical form. Permit an older history tick only under the explicitly valid post-death relation.

2. **`EffectSetCheckpoint` persists ring-buffer garbage and accepts non-canonical/invalid unused fields.**
   - Files: `src/modifiers/effect_set.cpp:196-270`, `src/modifiers/effect_set.cpp:284-289`
   - Capture copies all 16 backing command slots plus `command_head`, including commands already popped or left behind by `clear()`. Therefore two logically identical queues can produce different V9 bytes. Restore validates all slots only shallowly and accepts dirty `none` commands, invalid modifier enums when `has_modifier == false`, and dirty fields in unoccupied effect slots.
   - Fix: checkpoint the logical queue in deterministic order (`head = 0`, live commands packed, tail canonical zero), or canonicalize the runtime ring on pop/clear. Validate every enum before branching and require all semantically unused fields to equal their canonical defaults.

3. **Player and ordinary-attack checkpoints accept impossible or forged gameplay state.**
   - Files: `src/combat/room_combat_checkpoint.cpp:139-197`, `src/combat/room_combat_checkpoint.cpp:281-308`
   - `max_hp`, barrier caps, damage reductions, armor/evasion and their basis-point curves are only self-range checked rather than matched to the current durable build/abyss config. Attack startup/recovery durations, combo stage, hit latch, player state, and elapsed phase are not mutually checked. This permits an otherwise well-formed checkpoint to grant stats or create long/invalid attacks.
   - The active-skill normalization currently chooses ground `move` when horizontal velocity is nonzero (`:295-303`), while the approved rule is ground `idle` or airborne `jump_rise`/`jump_fall` after clearing the skill instance.
   - Fix: compare immutable derived player fields to the already initialized encounter build, validate exact attack duration/phase/state/combo/latch invariants, and normalize every ground active-skill capture to `idle` while preserving velocity separately.

4. **Monster validation trusts redundant derived fields instead of the immutable blueprint/config.**
   - Files: `src/combat/room_combat_checkpoint.cpp:199-227`, `src/combat/room_combat_checkpoint.cpp:396-416`
   - The blueprint comparison covers ID, affixes, and spawn only. `kind`, the complete `affix_profile`, `max_hp`, `max_break`, `max_shield`, and several timer/resource bounds can be changed independently and are restored verbatim. In particular, forged damage/move/cooldown basis points become live AI authority.
   - Fix: recompute the canonical monster definition/profile using the room blueprint and abyss config and require exact equality for every immutable/derived field before accepting mutable HP, reaction, AI, and effect state.

5. **Abyss and destructible-environment validation is incomplete.**
   - Files: `src/combat/room_combat_checkpoint.cpp:392-458`, `src/combat/room_combat_checkpoint.cpp:534-543`
   - Abyss restore checks only the rule enum; it accepts non-finite `locked_center`, a rule/active flag inconsistent with encounter config, and impossible cycle/stage values. Fixed obstacles accept nonzero HP/broken ticks while intact. Legacy fire crates only have `count <= 2` checked; expected count, fixed positions, `intact`/`broken_tick` relations, and inactive tail canonicality are unchecked, yet the entire crate array is copied regardless of count.
   - Fix: validate abyss state against its exact config/tick formula, validate every obstacle kind-specific invariant, and require the exact expected fire-crate count and immutable positions before applying only canonical entries.

6. **The new tests exercise mostly empty/transient-free happy paths.**
   - File: `tests/combat/room_combat_checkpoint_tests.cpp:16-95`
   - The round-trip test does not seed an active skill, projectile, hazard, hit stop, wrapped effect command queue, death snapshot, room field resident mutation, or obstacle mutation. It therefore did not detect the resident overwrite and skill-state softlock found during review. The invalid-RNG test checks only three world fields and does not establish general atomic rejection.
   - Fix: add the focused tests listed below before treating the foundation as complete.

### P2

1. **Checkpoint capacities are duplicated as literals.**
   - File: `src/modifiers/effect_set.hpp:62-81`
   - `EffectSetCheckpoint` hard-codes `8U` and `16U` before `EffectSet::kCapacity`/`kCommandCapacity` are declared. A future capacity change can silently desynchronize the checkpoint layout.
   - Fix: move the capacity constants to namespace scope and use them for both runtime and checkpoint arrays, with static assertions.

2. **There is an unused effect restore helper.**
   - File: `src/combat/room_combat_checkpoint.cpp:130-137`
   - `restore_effects()` is not called and duplicates the candidate-validation pattern used elsewhere.
   - Fix: remove it or use one shared checked conversion path.

## Required focused tests

1. Full room-field resident round-trip: mutate a current resident's HP/position/effects after capture, restore, and compare both persistent and active runtime state bit-for-bit.
2. Capture during both active skills on ground and in air; after restore, require skill cleared, cooldown preserved, ground state `idle`, airborne state correct, and movement/action input functional on the next tick.
3. Late-failure atomicity: inject an absent legacy ordinal and a residency/synchronization failure; require a full pre/post authority hash, RNG state, history, monsters, obstacles, and death state to remain identical when restore returns `false`.
4. History/death matrix: future history tick, `hp/death` disagreement, defense mismatch, noncanonical source, recent-damage mismatch, and valid long-post-death tick gap.
5. Effect queue wrap/pop/clear canonicalization plus invalid enums/dirty inactive slots; require identical encoded logical state for equivalent queues.
6. Derived-state tampering: player rating curve, attack phase/duration/combo, monster kind/profile/max resources, abyss center/stage/timer, fixed obstacle HP, and fire-crate count/position/tick.
7. Keep the stack/source guard that rejects whole-checkpoint/whole-large-array automatic temporaries, including aggregate assignment forms.

## Assessment

**Ready to merge: No.**

The primitive RNG/history APIs are sound and the three early blockers were corrected during review, but the foundation still has one atomicity P0 and several P1 acceptance gaps that would let malformed V9 state become live authority. Fix these before codec/worker integration hides them behind larger end-to-end failures.

---

## Final fix re-review (2026-07-27)

- Snapshot: current Task 5 foundation working tree on baseline `c6ecd4f`.
- Scope remained limited to RNG/history/effects and Combat room checkpoint capture/restore; worker/storage were not reviewed.
- Controller requested a read-only review without starting a build. The implementer separately reported `arpg_combat_tests: 283/283` passing after the final armor-state fix. This review also ran scoped `git diff --check`; it reported no whitespace errors (only the repository's existing LF-to-CRLF notices).

### Resolved P0

1. Restore now completes all fallible validation, lookup, resident planning, obstacle checks, and candidate effect/history imports before the first live assignment at `src/combat/room_combat_checkpoint.cpp:868`. There is no reachable `return false` in the publish section. Active residency is discarded before restored persistent records are installed, preventing old active state from overwriting checkpoint authority.
2. Death validation no longer performs signed arithmetic on untrusted defense values. Resource-loss bounds use the already validated player resources, so extreme `INT_MIN`/`INT_MAX` input is rejected without signed-overflow UB.

### Resolved P1

1. History/death/player authority is cross-validated: overflowed totals, invalid initialized/tick relations, post-death history drift, hp/death disagreement, defense/source mismatch, missing fatal recent damage, and inconsistent loss totals are rejected.
2. `EffectSetCheckpoint` now captures a canonical logical command queue, canonicalizes inactive data, validates every stored enum/unused field, and restores without preserving popped-ring garbage.
3. Player and attack restore now checks immutable derived stats, cooldown maxima, hurt/invulnerability and corrosion invariants, exact scaled phases/combo/state, finished attacks, latch/connected/event relationships, absent legacy ordinals, orphan attack state, active-skill normalization, and the valid near-landing `air_j` boundary.
4. Monster restore now checks blueprint-derived kind/profile/max resources in both field and legacy paths; shield recharge, effect ownership, break/armor two-state semantics, burning/blink timers, and affix-warning relations are bounded. In particular, `max_break > 0` accepts only `armored` with positive break value and zero window, or `broken` with zero break value and a 1..180 window.
5. Abyss restore now checks inactive-field canonicality and exact expansion/cycle timing against the last simulated authority tick, including both death-before-environment and death-after-environment cases and long post-death freezes. Fixed obstacles and legacy fire crates are validated against immutable identity, positions, count, hp/tick, collision, and canonical-tail rules.
6. Focused tests now seed declared transients and cover active-skill normalization, resident persistent/active restoration, late rejection atomicity, effect-ring canonicalization, cross-field tampering, active expansion/cycle and post-death gaps, near-landing air attack, malformed death/extreme defense, absent legacy latch ordinals, fire-crate tampering, and permanent-armor/timer tampering.

### Final verdict

- Remaining P0: **0**
- Remaining P1: **0**
- Merge-blocking P2: **0**
- Foundation verdict: **APPROVED**

This final verdict supersedes the earlier `Ready to merge: No` assessment for the foundation-only scope.
