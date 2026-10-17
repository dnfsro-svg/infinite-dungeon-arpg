# SDD ledger — plan: docs/superpowers/plans/2026-08-09-full-chinese-ui-foundation.md

Plan base: 8e8545ef8eaf420e4055a8897090a121e967f76d
Task 1: complete (commits 8e8545e..07e8c3f, review clean)
Task 2: in progress (base 07e8c3f)
Task 2: minor (deferred): zero-allocation probe observes C++ operator new, not arbitrary CRT malloc internals
Task 2: fix round 1/5 (1 addressed, 0 open — preserve exact-alias UTF-8 buffers and reject partial overlap; commits 65f0a53..eb77046)
Task 2: complete (commits 07e8c3f..eb77046, review clean)
Task 3: in progress (base eb77046)
Task 3: minor (deferred): glyph plan conflates invalid UTF-8 with capacity overflow
Task 3: minor (deferred): atlas byte multiplication needs an explicit 32-bit overflow policy if 32-bit targets are added
Task 3: review decision required — Task 3 plan sets instance count to 1 before Task 4 removes the three runtime LoadFontEx owners
Task 3: user ruling — keep the truthful runtime instance count at 3 in Task 3; Task 4 atomically changes ownership, monitoring, and the count to 1
Task 3: fix round 1/5 (2 addressed, 0 open — truthful staged instance count; production ASCII and 640/641 boundaries; commits 7ae6db5..1e15738)
Task 3: complete (commits eb77046..1e15738, review clean)
Task 4: in progress (base 1e15738)
Task 4: fix round 1/5 (6 addressed, 0 open — Stage16 API, unique owner, idempotence, GPU validation, runtime metrics, lifecycle and validator guards; commits ea8403e..d7cdbc1)
Task 4: complete (commits 1e15738..d7cdbc1, review clean)
Final review: with fixes — missing glyphs for ellipsis/list separator; UTF-8 copy is not prefix-preserving after capacity exhaustion; catalog translations diverge from approved design glossary
Final fix wave: in progress (base d7cdbc1)
Final fix wave: complete (commit e568b44; original 4 findings addressed)
Final fix re-review: 2 new Important findings open — legacy material consumer still needs glyph 蜕; monster affix short labels conflict with the approved downstream plan table
Foundation plan: BLOCKED pending user ruling on a second narrowly-scoped compatibility fix after the final-wave cap
User ruling: approved one extra narrowly-scoped compatibility fix beyond the final-wave cap
Compatibility fix: in progress (base e568b44)
Compatibility fix: complete (commit 567fe77; legacy glyph and exact short-label contract addressed)
Compatibility fix review: clean (no Critical/Important)
Foundation plan: complete (HEAD 567fe77; focused verification green)
