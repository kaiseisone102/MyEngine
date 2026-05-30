# SessionStart hook: clear the per-turn impl-gate marker (so the first edit of the
# new session is gated), then print SESSION_RULES.txt to stdout (plain stdout is
# injected as context for SessionStart per Claude Code hooks spec). Silent if missing.
$ErrorActionPreference = 'SilentlyContinue'
$marker = Join-Path $PSScriptRoot '..\.docs_read_marker'
if (Test-Path -LiteralPath $marker) {
    Remove-Item -LiteralPath $marker -Force -ErrorAction SilentlyContinue
}
$path = Join-Path $PSScriptRoot '..\SESSION_RULES.txt'
if (Test-Path $path) {
    Get-Content -LiteralPath $path -Raw -Encoding UTF8 | Write-Output
}
exit 0
