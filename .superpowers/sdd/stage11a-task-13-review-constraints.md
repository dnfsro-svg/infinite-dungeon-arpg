# Stage 11-A Final Review Constraints

Review the complete diff from `a06eddbdf557b7209146f12ec1a6bc75a1aa75b3` through HEAD.

- Check every design invariant across Combat, Dungeon, Persistence, Runtime/input, renderer/font, stress, formal validation, and evidence guards.
- Recheck same-tick death priority, frozen combat, both receipt matrices, not_committed retry, indeterminate fault, restart/no reroll, V5 to V6, abyss failure, floor-one clamp, permanent state preservation, and ground loss.
- Recheck zero-allocation implementation and reusable pending storage for all non-death item/abyss transactions and OOM behavior.
- Recheck five screenshots and evidence guards for actual production flow and false-positive/false-negative gaps.
- Check docs against current code/test names and results.
- Report every Critical, Important, and Minor finding; Stage 11-A cannot close with any outstanding finding.
