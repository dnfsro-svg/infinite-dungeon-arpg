# Task 2 review fix round 1 report

## Scope

- Tracked changes are limited to:
  - `tests/platform/hole_prompt_binding_source_test.cmake`
  - `tests/platform/hole_prompt_binding_source_self_test.cmake`
- No renderer or other production source was changed.

## Review finding and RED evidence

The reviewer finding was reproduced against the pre-fix guard:

- Exact self-test failed after 428.64 seconds with exit 1 because the guard
  rejected the valid `unrelated_auto_reference_is_valid` sample. This proved
  the whole-function `auto&&` ban was a false positive.
- Direct guard runs against the generated
  `door_requirement_in_runtime_false_branch` and
  `door_requirement_in_uncalled_auto_lambda` mutants both returned exit 0.
  Each mutant retains the exact required call only in a dead structure and
  uses the wrong live `{}` offset call, proving both false negatives.

## GREEN change

- Replaced the whole-function `auto&&` string ban with extracted-function-body
  structure checks.
- Constant-false detection covers `if(false)`, `if(0)`, and
  `if constexpr(false)` after lexical normalization.
- Lambda detection recognizes `auto`, `auto&`, and `auto&&` declarations and
  requires a later direct call or `std::invoke` call of the declared lambda.
- An unrelated legal `auto&&` reference remains accepted.
- Existing required live-path token anchors were not loosened or rewritten.

## Fresh verification

- Direct production-source guard: exit 0, 39.27 seconds.
- Exact self-test selector: 1/1 passed, 0 failures, test 433.25 seconds,
  total 433.26 seconds.
- `git diff --check`: no errors; only existing Windows LF-to-CRLF notices.
- Generated mutant readback confirms the two negative samples are
  syntactically valid function-body constructs and the positive sample is an
  ordinary reference declaration.

Per the review-fix instruction, no build, MENU/HUD selector, full/formal suite,
`Test.ps1`, Release build, or window run was performed because production code
did not change.
