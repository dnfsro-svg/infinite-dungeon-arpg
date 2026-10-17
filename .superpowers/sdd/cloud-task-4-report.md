# Cloud Task 4 Report

## RED

`python tests/cloud/cloud_readiness_test.py` failed before implementation because `.github/workflows/build-and-test.yml` did not exist. The other four contract tests passed.

## GREEN

Created the required workflow only. It runs Linux core setup/test work on Ubuntu, Windows debug tests and release build on Windows, then uploads the release `bin` output as `infinite-dungeon-arpg-windows-x64`.

## Local verification

- `python tests/cloud/cloud_readiness_test.py`
- YAML text inspection
- `git diff --check`

GitHub runner execution remains for the push-triggered cloud validation in Task 6.

## Independent review remediation

### RED evidence

After extending `tests/cloud/cloud_readiness_test.py`, the pre-fix workflow and configuration produced three expected contract failures from seven tests:

- Workflow had no top-level `permissions: contents: read` and used movable action tags.
- Windows job used `windows-latest` instead of `windows-2022`.
- `Configure.ps1` restricted `vswhere -products` to `Microsoft.VisualStudio.Product.BuildTools`.

### GREEN implementation

- Workflow pins Windows execution to `windows-2022`, declares read-only contents permission, and pins checkout/upload-artifact to the approved full SHAs with version comments.
- `Configure.ps1` now uses `-products '*'`, retaining the VS `[17.0,18.0)` range, x64 C++ tools, and SDK 26100 requirements. Its error message now accepts any qualifying VS 2022 product.
- Contract coverage now verifies runner selection, permissions, action SHAs, and the generic `vswhere` discovery constraints.

### Verification evidence

- `python tests/cloud/cloud_readiness_test.py`: 7 tests passed.
- `[System.Management.Automation.Language.Parser]::ParseFile(...)`: no PowerShell parser errors for `scripts/Configure.ps1`.
- YAML text self-check found the required permissions block, `windows-2022`, both SHAs, and version comments.
- `git diff --check`: passed.

Local execution does not simulate GitHub runners. Ubuntu and Windows hosted-runner execution remains Task 6 push validation.

## P2 contract-quality remediation

The cloud contract now reads the relevant workflow and PowerShell structures instead of using whole-file substring assertions:

- It requires one unindented `permissions:` block and permits only a two-space `contents: read` entry, ignoring blank lines and comments.
- A line-level parser reads genuine YAML `uses:` nodes only, requires all `actions/*` references to use 40-character hexadecimal SHAs, rejects `@v4`, and checks the exact checkout/upload refs.
- The PowerShell reader strips only line comments outside quotes, finds the unique string-only `$vswhereArgs = @(` array with a matching standalone `)`, and fails closed for unsupported syntax. It validates the generic product selector, retained range/components, and the unique `$installPath = & $vswhere @vswhereArgs` invocation.

Mutation self-tests are part of the two affected contract tests. They prove that a comment-only permissions/action decoy cannot restore a failed real action ref, and that a real Build Tools product rollback cannot be rescued by a comment containing `*`.

Verification: `python tests/cloud/cloud_readiness_test.py` passed all 7 tests; PowerShell AST parsing reported no errors; `git diff --check` passed.
