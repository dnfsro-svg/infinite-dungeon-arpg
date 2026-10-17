# Task 2 Review Round 2

Verdict: Changes requested

## Important

- If the build report exists but the canonical public door atlas, paired material alpha, schema, or recorded hash is invalid, the exporter silently falls back to the already-repacked ecology `(512,0)` wall cells and may publish walls as doors. Bootstrap from legacy atlas cells is allowed only when `environment-props-build.json` is completely absent; any invalid published state must fail closed before staging or replacing outputs.

## Minor ledger

- The C++ JSON reader checks 15 total records but does not separately assert five unique names per ecology. Python validation enforces exact ecology/object sets, so defer this non-blocking redundancy unless a later related edit makes it cheap.

## Closed from round 1

- Complete chaos wall, legacy-atlas bootstrap source, validate-before-replace publication, and real JSON-to-manifest cross-check.
