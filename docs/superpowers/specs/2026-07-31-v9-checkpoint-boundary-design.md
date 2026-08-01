# V9 Checkpoint Boundary Design

Date: 2026-07-31

## 1. Goal

Remove every Persistence dependency on Dungeon and Combat runtime headers and
symbols while preserving the V9 save bytes, validation strength, fixed
capacities, heap-only large-object policy, and gameplay behavior exactly.

This repair closes `architecture.persistence_checkpoint_only`; it does not add
a V10 format, change a field value/order, change a room or loot rule, change a
timeout, or alter save revisions.

## 2. Root cause

Commit `30fe24b` added the large-room V9 checkpoint. Its wire-format DTOs were
placed in `combat/room_combat_checkpoint.hpp` and
`dungeon/room_progress_checkpoint.hpp`. Persistence consequently includes or
calls into runtime layers:

- `room_progress_codec.cpp` includes health-potion and material-loot runtime
  headers for two schema constants/mappings;
- `room_progress_codec.hpp` and `save_commit_worker.hpp` include the Dungeon
  room-progress header for complete checkpoint types;
- `arpg_persistence` calls clear/compare/structural-validation functions whose
  implementations live in `arpg_dungeon` and `arpg_combat`.

The final game and current tests also link Dungeon, which hides the static
library's unresolved-symbol dependency. The source guard correctly exposes
the layering violation.

## 3. Considered approaches

### A. Replace the four direct includes

Forward declarations, duplicated constants, or a forwarding umbrella could
make the current text scanner green. The complete types and unresolved
Dungeon/Combat functions would still cross the boundary. This is rejected.

### B. Introduce a neutral checkpoint schema library

Create `arpg_checkpoint` under `src/checkpoint`. It owns stable DTOs, wire
capacities, ordinal mappings, clear/equality operations, and complete
wire-structural validation. Combat and Dungeon convert between live runtime
state and those DTOs; Persistence owns and serializes only the neutral DTOs.
This is selected because it makes the build graph and source graph agree.

### C. Add a second Persistence-only wire model

Keeping the current runtime checkpoint while creating a second large DTO at
the codec boundary would require copying multi-megabyte fixed arrays and
maintaining two field models. It increases stack/copy and drift risk and is
rejected.

## 4. Dependency architecture

The target graph becomes:

```text
core / abyss / skills / modifiers / items / progression / passives
                              |
                              v
                       arpg_checkpoint
                         /    |     \
                        v     v      v
                 arpg_combat  |  arpg_persistence
                        \     |
                         v    v
                       arpg_dungeon
```

`arpg_checkpoint` must not include or link `combat/`, Dungeon runtime,
Persistence, Platform, raylib, or App code. Persistence may include the stable
Dungeon-run DTO facade only through the neutral checkpoint headers; it may not
include any other `dungeon/` header and may not include `combat/` at all.

Root CMake registers lower targets first, then `src/checkpoint`, then Combat,
Dungeon, and Persistence, so the acyclic graph is explicit at configure time.

## 5. Neutral schema

### 5.1 Files

The neutral layer owns focused files:

- `checkpoint/dungeon_run_state.hpp`: the existing stable run, room, abyss,
  death, progression, passive, item, and skill state declarations now exposed
  by `dungeon/dungeon_checkpoint.hpp`;
- `checkpoint/room_combat_checkpoint.hpp`: checkpoint-only vector, enums,
  source/status/affix/environment/attack/monster/obstacle/death/history DTOs;
- `checkpoint/room_progress_checkpoint.hpp`: room lifecycle, ground records,
  `RoomProgressCheckpoint`, and `SaveCheckpointSlot`;
- `checkpoint/room_checkpoint_validation.hpp/.cpp`: clear, equality, and
  complete wire-structural validation;
- `checkpoint/room_checkpoint_schema.hpp`: schema capacities and ordinary,
  abyss, material, and health-potion ordinal mappings.

Every new DTO and schema function is declared in `arpg::checkpoint`.
Compatibility headers may expose `using` aliases in the existing
`arpg::combat` and `arpg::dungeon::checkpoint` namespaces, but new neutral and
Persistence code uses `arpg::checkpoint` directly.

Large room/save types remain non-copyable and non-movable. All fields use
fixed-width integers, fixed-capacity arrays, or already-neutral value types.
Every checkpoint enum has an explicit underlying type and explicit numeric
values matching V9.

The schema may reuse lower-layer values already serialized by V9, including
`items::ItemInstance`, `items::MaterialId`, and
`modifiers::EffectSetCheckpoint`. It may not reuse Combat runtime types merely
because their current layout happens to match the wire format.

Task 1 copies the current declarations and values into the neutral layer; it
does not edit or alias the legacy Combat/Dungeon headers yet. This temporary
duplication keeps every intermediate commit buildable. The legacy definitions
are removed only during the atomic production cutover, when their headers
become compatibility facades.

### 5.2 Compatibility facades

No existing file is deleted.

- `dungeon/dungeon_checkpoint.hpp` becomes the stable compatibility facade for
  `checkpoint/dungeon_run_state.hpp`.
- `combat/room_combat_checkpoint.hpp` exposes aliases for the neutral room
  combat DTO and declares only Combat capture/restore semantic adapters.
- `dungeon/room_progress_checkpoint.hpp` exposes aliases for the neutral room
  progress/save DTO and declares only Dungeon runtime semantic adapters.

Persistence includes neutral headers directly, so compatibility facades cannot
hide a transitive runtime dependency.

### 5.3 Buildable migration sequence

The migration must not commit a tree in which Dungeon tests pull an
incompatible old Persistence codec through their current link dependency.

1. add and test the independent neutral schema while all production consumers
   keep using the legacy DTOs;
2. add parallel Combat capture/restore overloads for the neutral combat DTO,
   while the legacy overload remains the production path;
3. in one reviewed cutover, turn the Combat and Dungeon headers into aliases,
   switch Dungeon and Persistence to the neutral values, and close both the
   source and isolated-link gates;
4. remove the temporary legacy overload in the same cutover, so two full room
   models are never a permanent architecture.

No intermediate commit may knowingly leave a configured test target unable to
compile.

## 6. Runtime adapters and validation

### 6.1 Combat

`CombatWorld::capture_room_checkpoint` writes directly into the neutral DTO.
`restore_room_checkpoint` validates and converts neutral enum/value fields into
live runtime state. Conversion is field-wise; it must not create another full
room checkpoint on the stack or heap.

Compile-time assertions bind every serialized runtime enum value and capacity
to its neutral counterpart. Unsupported values fail validation before any live
state mutation.

### 6.2 Dungeon

`DungeonSession` writes room ownership, ground records, room progress, and the
Combat adapter output directly into the neutral `SaveCheckpointSlot`. It owns
live-plan semantic checks involving room identity, generated blueprint hashes,
obstacle layout, and runtime configuration. The pure required-kills formula
`(generated_monsters + 3) / 4`, encoded ground ownership, item validity, and
abyss ownership remain in neutral structural validation so Persistence rejects
the same malformed payloads before publication. Dungeon consumes the same
neutral required-kills function instead of owning a second formula.

Material and health-potion checkpoint ordinal rules move to
`room_checkpoint_schema.hpp`; Dungeon loot code consumes the same definitions
instead of defining a second mapping.

### 6.3 Persistence

`room_progress_codec` accepts only neutral `SaveCheckpointSlot` values.
Decode uses a heap-owned scratch slot, validates every field/range/count and
the full neutral structural contract, and publishes only after the complete
payload and checksum pass. `SaveCommitWorker` embeds the neutral slot and links
only `arpg_checkpoint` plus existing lower libraries.

Persistence has an isolated link test whose executable links
`arpg_persistence` without Dungeon or Combat. This permanently prevents final
targets from hiding a reverse dependency.

### 6.4 Validation split

The neutral validator retains every rejection currently performed before V9
publish: canonical-none rules, nonzero RNG, counts/capacities, enum ranges,
finite vectors, ordered ordinals, bitsets, effect/history structure, attack
timeline consistency, death snapshot consistency, ground ownership, item
validity, and abyss ownership.

Pure encoded room bounds, required-kills, attack/status/death consistency,
player-damage-history rules, and affix interval/duration tables needed for
those checks are schema-owned data. Numeric enum/capacity contracts are bound
with `static_assert`. Attack- and affix-catalog values currently implemented in
`.cpp` files are compared exhaustively by Combat unit tests at runtime; the
plan does not claim an impossible cross-target `static_assert`. Effect and item
validation may call the already-lower `arpg_modifiers` and `arpg_items`
libraries. The neutral target never calls a Combat or Dungeon symbol.

Checks that require a live generated plan, obstacle layout, catalog object, or
current runtime configuration execute again in the Combat/Dungeon restore
adapter. Moving code between layers must not remove a rejection case from the
existing malformed-payload matrix.

## 7. Error and publication semantics

- Allocation failure remains explicit `std::nothrow` failure; no large fallback
  object is placed on the stack.
- Decode failure leaves the destination slot unchanged.
- Runtime restore validates before mutating the live world; failure preserves
  the existing fault/result mapping.
- Worker exact-verify and recovery behavior remain unchanged.
- No validation failure is converted into a warning or default value.
- No format fallback, timeout increase, golden rewrite, or guard exemption is
  permitted.

## 8. Compatibility invariants

The following must remain byte-for-byte or value-for-value identical:

- format version `9`, header, payload order, CRC, and maximum encoded size;
- all enum numeric values and scalar widths;
- room/monster/ground capacities and claim-word counts;
- material and potion checkpoint ordinals;
- V8/V9 decode routing and recovery selection;
- save revision, exact-verify, and clean-shutdown transaction semantics;
- captured/restored live state for normal, abyss, death-pending, cleared, and
  canonical-none rooms.

The repository did not contain a V9 golden before this work. Before any
production DTO is switched, deterministic V9 probes are checked in for
canonical-none, active-normal, started-abyss, death-pending with a nondefault
CombatDeathSnapshot, and cleared-abyss rewards. Each scenario freezes its exact
byte count and whole-stream SHA-256; active values deliberately exercise
attack/history/effects and ground records. CMake computes SHA-256 over every
emitted binary. Fixture sources, expected sizes, and digests become immutable
inputs for the refactor; they are not updated to make later changes pass. Fresh
Debug and Release encodes must match every manifest row.

## 9. Verification design

The implementation uses RED-to-GREEN gates in this order:

1. freeze the current pre-refactor deterministic V9 byte count and SHA-256;
2. existing `architecture.persistence_checkpoint_only` stays RED until
   Persistence has no forbidden include;
3. new `architecture.checkpoint_no_runtime_headers` rejects any neutral-layer
   include of Combat, Dungeon runtime, Persistence, Platform, raylib, or App;
4. new `persistence.link_isolation` compiles and links an executable against
   `arpg_persistence` alone, exposing unresolved reverse dependencies;
5. checkpoint schema tests cover canonical clear/equality, enum/capacity
   rejection, maximum population, ground records, death/history/effects, and
   allocation/stack policy;
6. the frozen V9 wire manifest plus existing malformed-payload, worker,
   recovery, Combat
   checkpoint, Dungeon capture/restore, and save-stack tests remain green;
7. focused Debug and Release serial builds/tests pass before full Task9 gates.

Every mutation self-test must fail for the intended diagnostic. No test may be
made green by excluding a source directory or linking Dungeon/Combat into the
Persistence isolation executable.

## 10. Task decomposition

Implementation is split into separately reviewable tasks:

0. freeze the current V9 fixture byte count and whole-stream SHA-256 before any
   production type changes;
1. add neutral schema DTOs, validation library, boundary guard, and schema unit
   tests without switching production consumers;
2. add parallel Combat capture/restore adapters and runtime catalog parity tests
   while preserving the legacy production path;
3. atomically adapt Combat/Dungeon facades and Dungeon/Persistence consumers,
   close the stricter Persistence source guard and isolated-link gate, and
   remove the temporary legacy Combat overload;
4. run wire-manifest, malformed, maximum-capacity, Debug/Release focused
   regressions and retain every compatibility facade.

Each task receives an independent specification review and code-quality review
before the next task begins. Production behavior is frozen throughout.
