$ErrorActionPreference = 'Stop'
$root = 'E:/game/.worktrees/integration-large-square-rooms'
$evidence = Join-Path $root '.superpowers/sdd/2026-07-26-prerequisite-feature-integration'
$env:ARPG_HEALTH_POTION_ONLY = '1'
& (Join-Path $root 'out/build/windows-msvc-core-debug/bin/arpg_dungeon_tests.exe') *>&1 |
    Set-Content -LiteralPath (Join-Path $evidence 'task-4-focused.log') -Encoding utf8
$code = $LASTEXITCODE
Set-Content -LiteralPath (Join-Path $evidence 'task-4-focused.exit') -Value $code -Encoding ascii
exit $code
