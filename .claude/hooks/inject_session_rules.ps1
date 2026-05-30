# SessionStart hook: print SESSION_RULES.txt to stdout (plain stdout is injected
# as context for SessionStart per Claude Code hooks spec). Silent if missing.
$ErrorActionPreference = 'SilentlyContinue'
$path = Join-Path $PSScriptRoot '..\SESSION_RULES.txt'
if (Test-Path $path) {
    Get-Content -LiteralPath $path -Raw -Encoding UTF8 | Write-Output
}
exit 0
