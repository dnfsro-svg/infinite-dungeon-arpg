# Task 9 review round 1

Verdict: Changes requested.

## Important

- `platform_key_pressed` combines raylib and Win32 with a short-circuit `||`. If raylib returns true, `GetAsyncKeyState` is not called and its low-bit edge can remain for the following frame, producing a duplicate NumPad skill press. Sample both paths unconditionally in the same call, then combine them. Do not alter the existing key-down semantics, and strengthen an existing Task 9 case without increasing the 473-case total.

## Other verified points

- Explicit `KEY_KP_1..5` array and exact Win32 `0x61..0x65` mappings.
- Main number row rejection, injected down-only behavior, unchanged map/submit signatures.
- Exactly four new cases, 469 to 473; Windows/non-Windows compile boundaries are correct.
- Independent `platform.units` passed with 473 cases; worktree and diff checks were clean.
