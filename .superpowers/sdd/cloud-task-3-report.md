# Cloud Task 3 Report

## Delivered

- Added `scripts/cloud/setup.sh` for fresh Linux core configuration, build, and test.
- Added `scripts/cloud/maintenance.sh` for incremental Linux core configuration, build, and test.
- Both scripts use Bash strict mode, verify `cmake`, `ninja`, and `g++`, and locate the repository from the script path.

## Verification

- RED: `python tests/cloud/cloud_readiness_test.py` initially failed because `scripts/cloud/setup.sh` was absent.
- GREEN: after implementation, the cloud script contract passes. The full contract has four passing tests and one expected out-of-scope failure: `.github/workflows/build-and-test.yml` is absent.
- Syntax: Git Bash `bash -n scripts/cloud/setup.sh` and `bash -n scripts/cloud/maintenance.sh` both passed.
- The Linux configure/build/test commands were not run on Windows.

## Scope

- Did not modify `.superpowers/sdd/task-5-report.md` or the CI workflow.
