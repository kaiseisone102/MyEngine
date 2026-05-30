# Stop hook: remove the per-turn impl-gate marker so the NEXT turn's first edit
# is gated again (user-chosen 2026-05-30: gate the first edit of every turn).
# Silent + always exit 0 so it never blocks the Stop event.
$ErrorActionPreference = 'SilentlyContinue'
$marker = Join-Path $PSScriptRoot '..\.docs_read_marker'
if (Test-Path -LiteralPath $marker) {
    Remove-Item -LiteralPath $marker -Force -ErrorAction SilentlyContinue
}
exit 0
