### Task 11: Final integration review and visual acceptance

**Files:**
- Modify only files required by review findings; every production fix requires a new RED regression.
- Record scratch evidence under ignored `.superpowers/sdd/`; do not commit generated build or acceptance artifacts.

**Interfaces:**
- Consumes all prior tasks.
- Produces reviewed `milestone/m04-monster-director` only; no `main` merge, push, PR, or Stage 5 branch.

- [ ] **Step 1: Run source-boundary scans**

Verify no raylib includes in `src/core`, `src/combat`, `src/dungeon`, `src/persistence`; no new `combat -> dungeon/persistence` or `persistence -> combat/dungeon` reachability; `git diff --check` produces no output.

- [ ] **Step 2: Run direct test executables in Debug and Release**

Run combat, dungeon, persistence, core, and platform executables individually. Every executable must exit 0 and report zero failures.

- [ ] **Step 3: Visual acceptance matrix**

Use isolated save directories and representative deterministic seeds. Confirm:

- all eight monster silhouettes and role labels;
- fire/water/lightning/chaos ecology colors;
- bomber, charge, dash, projectile, support shield, and hazard warnings before damage;
- player HP drops but never below 1 and restores only on committed room entry;
- one-wave and two-wave HUD/door behavior;
- normal hole remains sealed through all waves and ready after final clear;
- abyss remains presentation-only;
- F12 screenshot and window/Esc exit paths leave zero `arpg_game` processes.

- [ ] **Step 4: Independent whole-range review**

Review from `f3f442d` (Stage 4 design commit) to current HEAD against the design and this plan. Classify findings as Critical/Important/Minor. Fix all Critical/Important findings with RED tests, rerun Steps 1-3, and obtain a clean re-review.

- [ ] **Step 5: Final branch state**

```powershell
git status --short
git rev-parse main
git rev-parse milestone/m04-monster-director
git log --oneline --decorate --graph --max-count 40
```

Expected: worktree clean; `main` remains `bb74bd48b42435d798b2b6ce9968d1d2d3f8a9da`; Stage 4 branch contains only approved Stage 4 work. Preserve all M04 task worktrees and wait for the user's integration choice.

---

## Plan Self-Review Checklist

- [ ] All eight approved roles map to one stable catalog ID and one preferred ecology.
- [ ] Director defaults are exactly 8 base budget, +1 per 5 depths, cap 24, matching/off weights 4/1, two waves above 12.
- [ ] Normal/high-budget high-priority limits are 1/2 and active dangerous-area limit is 3.
- [ ] Every wave has a direct target and illegal all-support/all-ranged compositions have a deterministic fallback.
- [ ] Monster, projectile, hazard, wave, spawn, event, and diagnostic paths are bounded.
- [ ] Player HP clamp/reset is explicitly temporary and no death state is introduced.
- [ ] Same stable room data reproduces the same plan without changing ecology/hole/abyss RNG.
- [ ] Doors and holes remain sealed until the last wave clears.
- [ ] Abyss receives no Stage 4 gameplay multiplier or dedicated content.
- [ ] No rewards, XP, levels, equipment, loot, affixes, summoners, snipers, bosses, ECS, or Stage 5 work appears.
- [ ] Final review covers the full Stage 4 range and all Critical/Important findings are re-reviewed.
