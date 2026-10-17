# Task 5 worker / V9 persistence review

## Review boundary

- Baseline: `c6ecd4f090d47fdede97f4a601c9909eb61624cd`.
- Reviewed working-tree snapshot: 2026-07-27 06:42 -04:00.
- Read completely: `task-5-brief.md`, `task-5-preflight-review.md`, the current
  `save_commit_worker.*`, `room_progress_codec.*`, the V8 codec and A/B
  slot/recovery implementation, room-progress validation, and the current V9 /
  worker tests.
- This is a read-only static review. No source was edited and no build/test was
  run, as requested. This report is the only added file.
- `DungeonRuntime` and `DungeonSession` wiring was intentionally not reviewed.

## Conclusion

Not ready to integrate. The current code has the intended two 8 MiB buffers,
small sleeping worker, one queued request, and one-consumer completion shape,
but four P0 issues remain: slot ownership is not synchronized, A/B selection
trusts an unvalidated header word and can overwrite the sole valid save, the
supposed in-place readback decoder allocates and can terminate inside `noexcept`,
and the existing startup A/B loader cannot decode V9 at all.

## P0

### P0-1: no slot ownership state; producer can overwrite active or completed work

Evidence:

- `SaveCommitStorage::job()` exposes both mutable job slots directly
  (`src/persistence/save_commit_worker.hpp:64-68`).
- `submit()` checks only the numeric index and header revision. It never rejects
  `job_slot == active_slot_`, a slot already queued, or the slot held by
  `completion_` (`src/persistence/save_commit_worker.cpp:169-202`).
- The worker reads the submitted checkpoint without the mutex while encoding and
  decoding (`src/persistence/save_commit_worker.cpp:259-270,305-308`).
- A new request is accepted whenever `queued_slot_ == kNoSlot`, even while
  another slot is active or a completion is waiting
  (`src/persistence/save_commit_worker.cpp:185-190`).

The caller must fill a checkpoint before calling `submit()`. Therefore a caller
that chooses the active slot already races with worker encode/readback; a caller
that chooses the completion slot can destroy the exact checkpoint before the
completion is applied. This is undefined behavior, not merely a rejected/busy
edge case.

The queued-background supersede path has the same ownership hole. With active +
queued occupying both slots, an exact request must atomically cancel/reclaim the
queued background *before* recapturing into that slot. The current API requires
the caller to mutate the queued slot first and only then asks `submit()` to
supersede it.

Required fix: make slot ownership explicit (`free -> capturing -> queued ->
active -> completion_ready -> free`). Provide a mutex-protected capture-slot
acquire/reclaim operation and a tokenized submit. Once queued, neither the main
thread nor another submit may mutate the slot. Reclaiming a queued background
must first transition it back to `capturing`; active and completion-ready slots
must never be returned.

### P0-2: A/B arbitration trusts corrupt bytes and does not enforce monotonic revision

Evidence:

- `encoded_revision()` reads bytes 16..23 without checking magic, format, length,
  CRC, or structural validity (`src/persistence/save_commit_worker.cpp:96-105`).
- `commit()` maps missing, invalid, oversized, and temporarily unreadable files
  to revision zero, then selects the target solely from the two raw words
  (`src/persistence/save_commit_worker.cpp:276-286`).
- It never requires the submitted revision to be greater than the highest valid
  persisted revision and has no equal-revision identical/conflict rule.
- Post-publish verification reads only the target bytes; it does not rescan and
  arbitrate both A/B slots (`src/persistence/save_commit_worker.cpp:327-337`).

Concrete loss/corruption cases:

1. A corrupt slot with a forged high word is treated as active, so the worker
   overwrites the other slot even when that other slot is the only valid save.
   The corrupt high slot remains and still wins the next startup arbitration.
2. A stale request can be written to the older target and returned as
   `committed` even though a higher revision remains active.
3. Equal revision with different bytes creates an A/B conflict. This is reachable
   because queued background coalescing accepts `revision >= queued.revision`.
4. A transient read failure is treated like a missing/invalid slot and may cause
   the unknown (possibly newest) file to be chosen as the overwrite target.

Required fix: scan each existing slot into explicit
`missing/valid/invalid/unavailable` state using magic + length + CRC + structural
decode. V9 must arbitrate by nonzero `persistence_revision`; V1-V8 use validated
`commit_generation` as the migration baseline. Reject unavailable scans, reject
stale revisions, and define equal revision as committed/idempotent only when the
canonical bytes/decoded fields are identical. After replace, rescan both slots
and prove the target is valid, exact, and the selected highest slot.

### P0-3: worker readback is neither allocation-free nor atomic; bad allocation can terminate

Evidence:

- V9 durable decode calls the by-value legacy `decode_checkpoint()`
  (`src/persistence/room_progress_codec.cpp:950-952`). That function executes
  `items.resize(item_count)` (`src/persistence/checkpoint_codec.cpp:658-667`).
- The result is declared `const`, then `destination.state = durable.state` copies
  the item vector (`src/persistence/room_progress_codec.cpp:950,953-955`). This
  can allocate a second backing buffer.
- `decode_checkpoint_v9_into()` is `noexcept`; a vector-copy allocation failure
  at line 954 is not caught and therefore calls `std::terminate`.
- `clear_save_checkpoint_slot()` assigns an empty `DungeonRunState`
  (`src/dungeon/room_progress_checkpoint.cpp:152-155`), so it does not preserve a
  pre-reserved item backing contract.
- The worker decodes the readback into the submitted expected checkpoint itself
  (`src/persistence/save_commit_worker.cpp:305-308`). A later parse/validation
  failure leaves that supposedly immutable expected slot partially overwritten.

This violates the hard no-post-initialization-allocation rule and the immutable
job/readback exactness contract. The existing worker fixture has zero inventory,
so it does not expose the allocation path.

Required fix: initialize each slot with maximum bounded item backing, preserve
that capacity when clearing, and add an in-place durable decoder that writes only
within pre-reserved capacity. Worker readback should perform a streaming
structural-and-field comparison against the immutable expected slot (or use a
separately budgeted verification slot); it must not clear/overwrite the expected
job. Decode must validate into a complete destination before publishing it and
must return `allocation_failure`/capacity error rather than throw from a
`noexcept` function.

### P0-4: files written by the worker are not loadable by the current A/B startup path

Evidence:

- The current `read_slot()` still calls only `decode_checkpoint()`
  (`src/persistence/save_slot.cpp:167-199`), whose recognized magic ends at V8.
- `highest_slot()`, `load_recovered()`, and final transaction verification still
  arbitrate only `DungeonRunState::commit_generation`
  (`src/persistence/save_slot.cpp:272-295`,
  `src/persistence/save_recovery.cpp:147-171,215-247`).
- None of `save_slot.cpp`, `save_recovery.cpp`, or the slot detail types is
  modified in the reviewed snapshot.

Consequently a clean V9 file produced by this worker is classified invalid on
the next load; two V9 slots become recovery-required. This is persistence-layer
scope, independent of the intentionally unreviewed Runtime/Session wiring.

Required fix: add a V9-aware heap-slot load/scan path carrying
`persistence_revision`, migration status, and the room checkpoint; update A/B
highest/equal-conflict/final-scan rules to use that revision while retaining
V1-V8 migration.

## P1

### P1-1: the 32 MiB accounting excludes mandatory external backing

`kSaveCommitStorageResidentBytes` is only `sizeof(SaveCommitStorage)`
(`src/persistence/save_commit_worker.hpp:84-86`). Each job contains a
`std::vector<ItemInstance>` but `initialize()` never reserves it
(`src/persistence/save_commit_worker.cpp:109-125`). At the specified maximum,
two inventory backings alone require
`2 * 65535 * sizeof(ItemInstance) = 8,388,480` bytes, because
`sizeof(ItemInstance) == 64` (`src/items/item_types.hpp:61-68`). Dynamic backing
for the precomputed filesystem paths/config is also absent from the recorded
formula.

Record an exact inline + bounded external formula, reserve all required backing
during initialization, verify actual capacities, and hard-fail initialization
if the full resident amount exceeds 32 MiB. Add partial-allocation cleanup and
retry tests. A `sizeof`-only static assertion is necessary but insufficient.

### P1-2: coalescing accepts stale/equal replacements and completion lacks exact intent/epoch

- Any exact request supersedes a queued background even when its revision is
  lower; a background replaces another background on equal revision
  (`src/persistence/save_commit_worker.cpp:192-200`). Both violate unique,
  monotonic snapshot identity.
- `SaveCommitCompletion` carries only `background/exact`, not the exact
  transaction intent/token (`src/persistence/save_commit_worker.hpp:37-42`). A
  late exact completion cannot be tied to a particular full-clear/death/pickup/
  transition/shutdown request except by assuming revision uniqueness.

Require strict newer revision for replacement, or exact byte identity for an
idempotent equal revision. Carry a stable request intent/token and worker epoch
through queued work and the one-shot completion.

### P1-3: `stop_and_join()` can leave a stale completion after restart

`stop_and_join()` clears `completion_ready_` before joining
(`src/persistence/save_commit_worker.cpp:215-223`), but an already active worker
can finish afterward and set it true (`src/persistence/save_commit_worker.cpp:249-255`).
The post-join block resets only `started_` and `active_slot_`
(`src/persistence/save_commit_worker.cpp:224-228`). A later `start()` therefore
inherits an old completion; the worker will not process a queued job while that
flag is set, and `try_take_completion()` may expose the pre-stop receipt.

After join, invalidate completion/queue/slot ownership and advance an epoch (or
make the worker non-restartable). Tests must cover stop during active I/O,
stop/restart, repeated stop, queued cancellation, and exactly-once slot release.
Integration must still guarantee worker stop/join before storage teardown; the
raw `SaveCommitStorage*` does not enforce that lifetime itself.

### P1-4: publish is missing directory durability and full error classification

The Windows path correctly writes a same-directory temp, calls
`FlushFileBuffers(file)`, and uses replace + write-through
(`src/persistence/save_commit_worker.cpp:24-45,77-81`). It does not flush the
parent directory after the rename. The brief requires durable temporary-file
write/flush/replace/readback; the review assignment explicitly requires a
directory flush.

Also, `write_file()` collapses write, file flush, and close failure to `false`,
which the caller reports as `write_failed`; the declared `flush_failed` error is
never produced. The worker ignores the existing `final_scan_a/final_scan_b`
fault points and does not distinguish an unavailable slot scan from an invalid
slot.

Add an explicit documented Windows directory-durability step (or document and
test the platform-specific strongest available guarantee), preserve
write/flush/replace/readback error categories, and perform the two-slot final
scan before returning `committed`.

### P1-5: V9 structural validation is incomplete and decode is not transactional

`valid_room_progress_checkpoint_structural()` checks counts/order and only a
small subset of combat fields. It does not validate player/attack enums and
ranges, effect checkpoints, abyss runtime, damage-history tick relation,
obstacle/death fields, or all finite/collision constraints
(`src/dungeon/room_progress_checkpoint.cpp:158-246`). The encoder relies on this
function before looping `fire_crate_count`, but that count is not checked before
indexing the fixed array (`src/persistence/room_progress_codec.cpp:629-667`). An
invalid source can therefore cause an out-of-bounds read during encode.

The decoder clears and starts mutating `destination` before room-progress parsing
and final validation (`src/persistence/room_progress_codec.cpp:953-964`), so an
error does not preserve the previous destination.

Use one shared field validator covering every serialized field and capacity.
Worker structural decode/compare must reject malformed enums/effects/history/
death without world/RNG work; main-thread semantic validation remains the
separate rules/blueprint/collision layer. Publish a decoded slot only after all
structural validation succeeds.

### P1-6: current tests do not exercise the acceptance gates

The worker suite contains only an empty-inventory commit and one coalescing
scenario (`tests/persistence/save_commit_worker_tests.cpp:75-176`). Missing are:

- active/completion slot overwrite rejection and bounded reuse;
- equal/stale/overflow revisions and exact-lower-than-background rejection;
- valid + corrupt-high A/B, unreadable slot, equal-conflict, and V8->V9 baseline;
- post-publish two-slot selection and exact readback corruption;
- non-empty/max inventory with an allocator disabled after initialization;
- external resident-byte accounting and partial allocation/start failure cleanup;
- idle sleep, single-consumer repeated poll, stop during active work, restart,
  stale completion, and exact intent matching;
- all declared fault points and precise `not_committed`/`indeterminate` results.

`tests/persistence/CMakeLists.txt:21-24` says a
`persistence.save_commit_stack_guard` covers large automatic objects, but no such
test is declared in the file and the planned guard script/storage tests do not
exist in the reviewed snapshot. The no-raylib/no-live-authority source guard is
also absent.

### P1-7: the existing equality helper is not an exact checkpoint comparator

`same_room_progress_checkpoint()` compares only selected room/combat summary
fields and ground records; it omits player, attack, monster records, obstacle
records, history buckets, abyss runtime, and death contents
(`src/dungeon/room_progress_checkpoint.cpp:249-297`). It is not currently used by
the worker, but it must not be reused as the planned field-wise readback exact
comparison without being completed. Byte equality is useful but does not replace
structural decode plus exact field comparison for the specified contract.

## P2

### P2-1: no static maximum encoded-size formula

The codec defines only the 8 MiB ceiling (`src/persistence/room_progress_codec.hpp:11-13`).
It needs a fixed schema-size formula for the V8 durable maximum plus all V9
monster/equipment/secondary/environment/history/effect records and a
`static_assert(kV9MaximumEncodedBytes <= 8 MiB)`. Otherwise a future legal field
addition can silently make maximum-capacity saves unencodable.

### P2-2: out-of-range storage access silently aliases slot zero

`job()`, `buffer()`, `slot_path()`, and `temp_path()` map every invalid index to
zero (`src/persistence/save_commit_worker.cpp:128-149`). `submit()` later rejects
the invalid numeric index, but the caller may already have overwritten job zero
through `job(invalid)`. Return a nullable/result handle or enforce the index
contract with a checked assertion; do not silently alias live storage.

### P2-3: initialization and stale-temp lifecycle are under-specified

`SaveCommitStorage::initialize()` can be called again and mutate paths/config;
there is no guard against reinitialization after worker start. Crash-left temp
files are not inspected at initialization, and each commit removes only the temp
for its chosen target. Define one-shot initialization (or safe retry before
start), and make the V9 startup scan own deterministic temp recovery/removal.

## Verified positive properties in the reviewed snapshot

- Exactly two inline 8 MiB codec buffers exist in `SaveCommitStorage`.
- `SaveCommitWorker` holds only a storage pointer and small synchronization /
  state members, with `static_assert(sizeof(SaveCommitWorker) <= 1024)`.
- No large automatic checkpoint/codec array is present in the reviewed worker or
  V9 codec translation units.
- The idle loop uses a predicate condition-variable wait; it does not poll.
- A ready completion is protected by the mutex and cleared by the first
  `try_take_completion()`, so two simultaneous consumers cannot both take the
  same receipt (excluding the stop/restart stale-completion defect above).
- An active background is not canceled; an exact request can wait in the sole
  queue, and a queued exact is not displaced by a background request.
- The Windows path uses a same-directory temp file, file-content flush,
  replace-with-write-through, and pre/post-publish byte readback.
- V9 uses `ARPGSV9\0`, explicit little-endian scalar writes, payload length, CRC,
  and a hard decode-size ceiling. The worker source contains no raylib, gameplay
  tick, live `DungeonSession`, or RNG-generation call.

## Static re-review (2026-07-27 09:49 -04:00)

### Boundary and verdict

- Re-reviewed the latest stable working-tree snapshot at baseline
  `c6ecd4f090d47fdede97f4a601c9909eb61624cd` after the implementer paused Task 5
  edits. Representative blob ids are `c0c5ba3` (`save_commit_worker.cpp`),
  `39a1763` (`room_progress_codec.cpp`), `3df40b5`
  (`checkpoint_codec.cpp`), `88b2e72` (`room_progress_checkpoint.cpp`), and
  `e663551` (`checkpoint_codec_tests.cpp`).
- This re-review remained read-only for production/test sources. No build or
  test was run by this reviewer; this appended section is the only review edit.
- **Current worker/V9 verdict: NOT APPROVED. Remaining original findings are
  P0 = 0, P1 = 5, P2 = 3.** All four original data-race/data-loss/termination/
  unloadable-file P0 defects now have closing source evidence, but the hard
  durability, accounting, validation/equality, and acceptance-test gates are
  not complete.
- The implementer independently reported the two executables passing
  (`persistence` 100/100 and `platform` 495/495) and the new stack guard passing.
  The latest full CTest run nevertheless has two failures:
  `platform.input_latency_source` and `platform.host_input_source`, both on the
  required Stage11B -> Stage11C -> Stage11D -> mapping host-input order. These
  are external execution results, not independently reproduced by this review.

### Original P0 matrix

| Finding | Status | Closing evidence / remaining boundary |
|---|---|---|
| P0-1 slot ownership race | **Closed** | `free -> capturing -> queued -> active -> completion_ready -> free` is explicit; acquire/capture/submit/cancel validate slot, token and epoch, and queued/background reclaim happens under the mutex (`save_commit_worker.hpp:26-49,145-156`; `save_commit_worker.cpp:322-447`). Active and completion-ready slots are never returned for capture. |
| P0-2 raw A/B revision arbitration | **Closed at P0 severity; residual P1 below** | Initialization reads bounded bytes, decodes V9 or V1-V8 structurally, carries the validated persistence revision, rejects unavailable slots, selects the highest revision, and treats equal-revision different bytes as a two-file conflict (`save_commit_worker.cpp:114-200,250-286`). Stale revisions are rejected and equal revision is idempotent only for an exact byte-identical request (`save_commit_worker.cpp:505-539`). The legacy heap loader and recovery arbitration are also V9-aware (`save_slot.cpp:171-238,310-330`; `save_recovery.cpp:147-207`). |
| P0-3 allocating/mutating worker readback | **Closed** | Ownership validation now uses caller-provided byte scratch with an allocation-free open-addressing table (`item_catalog.cpp:503-583`), and V8 encoding invokes it only after proving output capacity (`checkpoint_codec.cpp:328-365`). Worker readback keeps the immutable job intact and streams every V8 durable and V9 room field through exact comparison (`checkpoint_codec.cpp:526-765`; `room_progress_codec.cpp:1144-1223`). Public materializing V9 decode uses a heap scratch, reserves destination inventory before publishing scalars, and publishes the fixed room state only after complete validation (`room_progress_codec.cpp:1108-1141`). The max-65,535-item allocator-failure sentinel covers V8 caller encode/field verify and V9 encode/readback (`checkpoint_codec_tests.cpp:1405-1443`). |
| P0-4 V9 not loadable on startup | **Closed** | Both `SaveCommitStorage::initialize()` and legacy `read_slot()` recognize V9, retain `persistence_revision`, and keep V1-V8 migration (`save_commit_worker.cpp:138-160`; `save_slot.cpp:206-238`). Startup/recovery compare the persistence revision rather than V8 `commit_generation`, including equal-revision conflict handling. |

### Original P1 matrix

#### P1-1 exact resident accounting — **Open**

`kSaveCommitStorageResidentBytes` now adds two maximum inventory backings and
`initialize()` reserves both (`save_commit_worker.hpp:130-133`;
`save_commit_worker.cpp:125-130`). This is still a lower-bound formula, not the
specified exact resident amount:

- it assumes `vector::capacity() == reserve request` although the code checks
  only `>=`;
- it excludes dynamic backing owned by the four `filesystem::path` values and
  copied `SaveStoreConfig::directory`;
- no runtime capacity/allocation accounting proves the permanent total, no
  exact resident/peak assertion exists, and partial-allocation cleanup/retry is
  not covered by a storage test.

#### P1-2 coalescing / intent / epoch — **Closed**

Replacement requires a strictly newer revision and only reclaims queued
background work. Completion carries revision, kind, intent, token and epoch;
submit and receipt paths validate the lease identity (`save_commit_worker.cpp:
322-423`). Runtime additionally matches exact/background flights before applying
receipts. An equal/stale replacement is no longer silently accepted.

#### P1-3 stop/restart stale completion — **Closed**

Stop cancels queued ownership, suppresses an active result while stopping,
joins, then clears completion, all slot states/tokens and queue/active indices;
restart advances the epoch (`save_commit_worker.cpp:301-319,425-485`).

#### P1-4 durability, final arbitration, and error classification — **Open**

- Windows write still collapses `WriteFile`, `FlushFileBuffers`, and
  `CloseHandle` failure into one boolean, so the declared `flush_failed` result
  is unreachable and is reported as `write_failed`
  (`save_commit_worker.cpp:26-47,488-556`).
- Replace uses `MOVEFILE_WRITE_THROUGH`, but there is no explicit parent
  directory durability operation or documented/tested platform guarantee after
  the rename (`save_commit_worker.cpp:79-83`). Delete/cleanup failures are also
  ignored.
- The post-publish target is exact-field verified, but the final loop merely
  reads a cached-valid other slot. It does not parse/CRC/structurally validate
  that slot, refresh its revision, or prove final two-slot highest/equal-conflict
  arbitration (`save_commit_worker.cpp:575-608`).
- An oversized existing file makes bounded `read_file()` return false and is
  classified `unavailable/blocked`, rather than structurally invalid and
  recoverable (`save_commit_worker.cpp:49-76,138-164`).
- `SaveCommitWorker::start()` rejects a still-held loaded slot but does not
  reject `blocked` or `recovery_required` storage after a caller releases/no
  longer has a loaded index (`save_commit_worker.cpp:301-304`). The production
  runtime avoids this path, but the public storage/worker contract does not.

#### P1-5 shared structural validator — **Open, substantially reduced**

The active/death checkpoint validator now checks player/attack, RNG, history,
monster enums/ranges/effects, obstacles/fire crates, death/defense, owner and
abyss cross-invariants before active restoration. The original count-before-
indexing and partially published V9 decode defects are closed.

One serialized-state hole remains: lifecycle `none` returns early through
`canonical_none()`, which checks only tick/RNG/counts/history header/death flag
for combat. It does not require the serialized player, attack, abyss runtime,
history buckets, or other fixed combat fields to be canonical zero/valid
(`room_progress_checkpoint.cpp:59-68,225-240,319-324`). Thus a CRC-valid V9
`none` record can carry malformed enum/state bytes that bypass the shared combat
validator, contrary to the requirement that all serialized fields be validated.

#### P1-6 acceptance tests and source guards — **Open**

The stack guard is now a real CTest and catches direct automatic declarations of
the named large types/buffer (`tests/persistence/CMakeLists.txt:31-36`;
`save_commit_stack_guard_test.cmake:5-32`). The max inventory allocation-failure
codec test is also valuable. The acceptance matrix is nevertheless incomplete:

- the worker registry still contains only three cases: basic exact commit, one
  active+queued supersede scenario, and equal-revision archive
  (`save_commit_worker_tests.cpp:89-245`);
- no worker test covers active/completion overwrite rejection, bounded reuse,
  stale/equal/overflow revision matrix, corrupt-high/unavailable/V8 baseline,
  max inventory with allocation disabled after **worker initialization**, exact
  resident accounting, partial initialize/start failure, idle sleep, repeated
  single-consumer poll, stop during active I/O/restart, or every fault/result
  classification;
- the source guard does not enforce absence of raylib, live `DungeonSession`,
  RNG generation, gameplay ticks/events, or other live-authority calls in the
  worker translation unit;
- the Task 5 completion schedule matrix (before/after capture, after 1/7/31
  presented frames, active-background queuing, pause/inventory zero-fixed-tick
  pump, stale receipt injection) and the at-least-10,000-authority-tick
  save/reload equivalence test do not exist. Poll-until-complete helpers do not
  prove these schedules;
- the latest full CTest run has the two host-input source-order failures recorded
  above, so the current whole gate is not green even aside from missing tests.

#### P1-7 exact room-progress equality helper — **Open**

The worker correctly stopped relying on this helper and instead uses the full
streaming verifier. However, the public `same_room_progress_checkpoint()` still
compares only summary/bitsets/counts, a few combat headers, and ground records.
It omits player, attack, full monsters, obstacles, fire crates, damage-history
buckets, abyss runtime and death contents (`room_progress_checkpoint.cpp:
428-476`). It remains unsafe as an exact checkpoint comparator and has no
direct completeness test.

### Original P2 matrix

| Finding | Status | Remaining evidence |
|---|---|---|
| P2-1 fixed maximum encoded-size formula | **Open** | `room_progress_codec.hpp:11-13` still defines only the 8 MiB ceiling. There is no schema-derived V8 + max monster/equipment/secondary/environment/history/effect formula and no `static_assert(kV9MaximumEncodedBytes <= 8 MiB)`. |
| P2-2 invalid access aliases slot zero | **Partially fixed, still open** | `job(invalid)` is now nullable and the mutable public buffer accessor is gone (`save_commit_worker.cpp:207-210`). `slot_path(invalid)` and `temp_path(invalid)` still silently return path zero (`save_commit_worker.cpp:212-220`). |
| P2-3 initialization/temp lifecycle | **Partially fixed, still open** | Successful initialization is one-shot and attempts to remove both stale temps. Removal has no success result, an undeletable temp does not fail initialization, and partial initialization/retry/temp failure behavior is not tested (`save_commit_worker.cpp:114-136,199-204`). |

### Approval conditions

Do not integrate this snapshot yet. Approval requires, at minimum:

1. close the five P1 items above, including exact resident accounting, durable
   publish/error semantics, canonical `none` validation, the complete equality
   helper, and the full worker/schedule/10k/source-guard matrix;
2. make the complete required CTest selection green, including the two current
   host-input source-order guards;
3. retain the now-closed four P0 properties under allocator-disabled maximum
   inventory and stop/restart/fault-injection coverage.

Until those conditions are met, the explicit decision is **NOT APPROVED**.

## Final static re-review of the claimed closure (2026-07-27 10:55 -04:00)

### Boundary and verdict

- Re-reviewed the frozen working-tree snapshot at baseline
  `c6ecd4f090d47fdede97f4a601c9909eb61624cd`. Representative current blob ids
  are `9f04938` (`save_commit_worker.cpp`), `a1610b0`
  (`save_commit_worker.hpp`), `686f574` (`room_progress_codec.cpp`), `481ddb6`
  (`room_progress_codec.hpp`), `22ebe21`
  (`room_progress_checkpoint.cpp`), `a6e7735`
  (`save_commit_worker_tests.cpp`), `e964dc3`
  (`dungeon_runtime_tests.cpp`), and `09e0c89`
  (`checkpoint_v9_tests.cpp`).
- This reviewer did not modify production or test sources and did not run a
  build or test. This appended review section is the only edit.
- The implementer reports `persistence` 100/100 in 83.36 seconds, `platform`
  495/495 in 16.83 seconds, plus the stack and both host-input source guards
  passing. Those results were not independently reproduced here.
- **Current worker/V9 verdict remains NOT APPROVED: P0 = 0, P1 = 2, P2 = 0.**
  The previous resident-accounting, canonical-none, exact-comparator,
  size-formula, invalid-access, initialization/temp, source-guard, schedule,
  and 10,000-tick findings are closed. Two acceptance boundaries remain.

### Closed findings confirmed in this snapshot

- Resident bytes are computed from the two exact reserved inventory capacities
  and the five actual path capacities, checked against both the schema bound
  and 32 MiB, and reset completely on partial initialization failure
  (`save_commit_worker.cpp:19-77,230-335`; `save_commit_worker.hpp:130-138`).
- Invalid job/path indices return null, stale-temp deletion is mandatory,
  oversized and unavailable files are distinguished, and blocked/recovery
  storage cannot start a worker (`save_commit_worker.cpp:247-268,338-355,
  437-458`).
- The canonical `none` validator covers all serialized fixed combat state, and
  `same_room_progress_checkpoint()` now compares every serialized field with
  bit-exact float equality. Direct mutation checks cover signed zero, attack
  state, and the final damage-history bucket
  (`room_progress_checkpoint.cpp`; `checkpoint_v9_tests.cpp:326-350`).
- The field-derived maximum V9 size includes the V8 maximum inventory plus all
  fixed-capacity room/combat/effect/environment/ground records and is statically
  bounded by 8 MiB (`room_progress_codec.hpp:15-83`).
- Temp write/flush/close/readback/cleanup errors are distinct, the target is
  exact-field verified, and the normal new-revision path rereads both final
  slots before reporting committed (`save_commit_worker.cpp:99-175,640-842`).
- The worker tests cover ownership leases, superseding, repeated single-consumer
  polling, bounded reuse, stale/equal/max revisions, stop/restart, stale leases,
  maximum inventory under armed allocation failure, I/O fault categories,
  conflict recovery, corrupt-high, V8 migration, oversized, unavailable, and
  temp-cleanup retry. The source guard rejects large automatic storage as well
  as raylib, live session, RNG-generation, fixed-tick, and event calls.
- The eight runtime schedules are real registered test work: completion before
  capture, stale completion after capture, 1/7/31 zero-authority-tick presented
  frames, and an active background job. Every schedule compares saved V9 bytes,
  next-tick V9 bytes, and event order (`dungeon_runtime_tests.cpp:1432-1630`).
- The 10,000-tick test performs real V9 encode, decode, and session restore,
  then compares exact V9 bytes/authority CRC, full room-progress equality, RNG
  state, damage-history totals, obstacles/fire crates, and both event streams on
  every following authority tick (`checkpoint_v9_tests.cpp:158-248,362`).

### P1-1: not every `committed` result performs final two-slot arbitration

The equal-revision exact-idempotent branch rereads only the selected active
slot and returns `committed` immediately (`save_commit_worker.cpp:667-680`). It
does not execute the final A/B scan at lines 747-839. A second slot changed after
initialization to a higher revision or to a same-revision conflicting image can
therefore be ignored while the worker reports success. This violates the prior
approval condition that every committed result be based on final two-slot
arbitration.

The normal final scan also does not prove the comment's claimed exact identity
for the untouched prior-valid slot. For V9 it validates only the outer envelope
and then compares format, size, revision, and a 32-bit whole-file CRC; for a
legacy slot it reuses the cached revision and compares the same metadata
(`save_commit_worker.cpp:776-818`). CRC32 equality is not byte identity, and an
outer-envelope CRC does not validate the decoded room/durable structure. A
changed same-size image with recomputed envelope CRC (or a CRC collision) is not
an exact comparison to the image structurally validated at initialization.

Route the idempotent case through the common final A/B arbitration. For the
untouched slot, either retain and byte-compare the originally validated image,
or rerun a complete allocation-free structural decoder/validator and fresh
revision arbitration; size/CRC metadata alone is insufficient for an exact
durability gate.

### P1-2: deterministic worker-start failure cleanup/retry is still untested

`SaveCommitWorker::start()` catches thread-construction failure and appears to
leave a retryable stopped object (`save_commit_worker.cpp:437-458`), but the
Task 5 acceptance requirement explicitly calls for worker-start failure cleanup.
The current worker suite tests blocked/recovery start rejection, not a forced
thread-construction failure followed by a successful retry
(`save_commit_worker_tests.cpp:124-582`). Add a deterministic start-failure
injection (or equivalent thread-factory seam), assert no running/joinable worker,
no leased slot or stale completion, then retry and complete one save.

### Approval conditions

Do not integrate this snapshot yet. Approval requires:

1. make equal-revision idempotence use the same final two-slot scan as a normal
   commit and replace CRC-only prior-slot identity with byte-exact comparison or
   complete allocation-free structural revalidation;
2. add and pass deterministic worker thread-start failure cleanup and retry
   coverage;
3. rerun the already-green persistence/platform/stack/host-input gates without
   regressing the confirmed schedule and 10,000-tick evidence.

Until those two remaining P1 findings are closed, the explicit decision is
**NOT APPROVED**.

## Closure re-review (2026-07-27 11:14 -04:00)

### Boundary and final verdict

- Re-reviewed only the two remaining findings against the updated frozen
  working tree at baseline `c6ecd4f090d47fdede97f4a601c9909eb61624cd`.
  Current relevant blob ids are `f5e179a` (`save_commit_worker.cpp`), `a1610b0`
  (`save_commit_worker.hpp`), `29b7dfc` (`save_store.hpp`), and `af23ffd`
  (`save_commit_worker_tests.cpp`).
- This was a read-only production/test-source review. No build or test was run
  by this reviewer, and this appended report section is the only edit.
- The implementer reports the updated `persistence` executable passing 100/100
  in 82.55 seconds and `platform` passing 495/495 in 15.27 seconds. These
  execution results were not independently reproduced here.
- **Final worker/V9 verdict: APPROVED. P0 = 0, P1 = 0, P2 = 0.** This section
  supersedes the preceding NOT APPROVED verdict.

### P1-1 final two-slot byte-exact arbitration — **Closed**

- Initialization fully decodes each bounded valid slot and retains that exact
  original file image in its dedicated fixed buffer
  (`save_commit_worker.cpp:294-323`). Thus a legacy mirror is backed by a full
  successful legacy decode and a V9 mirror by a full successful V9 decode.
- `compare_file()` checks exact length and then compares every byte in bounded
  4 KiB chunks, including close failure; it no longer uses CRC32 as an identity
  proof (`save_commit_worker.cpp:164-195`).
- Before a new-revision commit mutates a target mirror, every cached valid disk
  slot is byte-compared with its validated mirror. The target buffer is then
  replaced only by the canonical encoder output. Temp readback, published
  target readback, the untouched slot, and the final A/B scan all use exact byte
  comparison (`save_commit_worker.cpp:711-778,804-929`).
- Equal-revision exact idempotence no longer returns early. It first byte-checks
  both disk slots, requires active format V9, runs full allocation-free
  `verify_checkpoint_v9_readback()` against the immutable job, and then invokes
  the same final-scan hooks, byte-exact A/B checks, highest-revision check, and
  equal-revision conflict arbitration before returning committed
  (`save_commit_worker.cpp:779-802`).
- Both and only committed exits are now after successful `final_scan()`
  (`save_commit_worker.cpp:793-802,918-931`). Tests force `final_scan_b` on the
  equal-idempotent path and require `indeterminate/final_scan_failed`; they also
  replace the other slot after initialization with a valid higher revision and
  require the equal request not to commit
  (`save_commit_worker_tests.cpp:627-695`).

### P1-2 deterministic worker-start cleanup/retry — **Closed**

- `SaveFaultPoint::worker_start` is evaluated exactly at the thread-creation
  boundary. Both the deterministic hook and a real `std::thread` construction
  exception use one rollback path restoring the previous epoch, free slot
  states, zero tokens, no active/queued slot, empty completion, and stopped
  flags (`save_store.hpp:41-54`; `save_commit_worker.cpp:475-514`).
- The registered worker test forces that failure, proves the worker remains
  stopped and storage/resident state unchanged, retries the same instance,
  commits at epoch 1, stops/restarts, and obtains epoch 2
  (`save_commit_worker_tests.cpp:189-230,700-705`).

### Final decision

All original P0/P1/P2 findings and the Task 5 acceptance evidence are now
closed in the reviewed snapshot. The byte mirrors remove the CRC-collision and
legacy-revalidation gap without adding a third large buffer; every successful
commit has final A/B arbitration; and worker-start rollback now has a
deterministic registered test. **Task 5 is approved for integration, subject to
the implementer's reported green test results being preserved by the actual
integration commit.**
