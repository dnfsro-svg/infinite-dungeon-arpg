# SDD ledger — plan: docs/superpowers/plans/2026-07-31-task9-fixture-stack-recovery.md

Task 1: RED confirmed — both standalone fixtures exit 0xC00000FD before the fix; Windows Application Error event 1000 confirms stack overflow.
Task 1: implementation commit ab9f888 — both PE stack reserves read back as 0x400000; direct runs no longer exit 0xC00000FD and produced no matching Application Error 1000.
Task 1: review — spec noncompliant and quality needs fixes because both exact behavioral CTests remain 0/1 after the stack fault is removed; no Critical findings.
Task 1: minor (correcting): implementer report incorrectly described the newly added plan as already tracked and unchanged.
Task 1: plan conflict — making the two behavioral CTests green would require work outside this card while the binding constraints prohibit fixture, rule, value, timeout, production, and B-G changes. Standing user authorization selects the recommended safe split: retain the independently verified stack prerequisite, do not claim fixture recovery complete, and diagnose Stage 9 and Stage 10 as separate follow-up cards.
