# PreToolUse hook (Edit|Write|MultiEdit): per-turn first-edit gate + checklist.
#
# Behaviour (user-chosen 2026-05-30): the FIRST source edit each turn is BLOCKED
# until a marker exists; once the marker is created the rest of that turn's edits
# pass freely. The marker is removed by the Stop hook (clear_docs_marker.ps1) and
# at SessionStart (inject_session_rules.ps1), so each new turn re-gates once.
#
#   marker MISSING -> permissionDecision = deny (edit blocked) + how-to-unblock.
#   marker PRESENT -> permissionDecision = allow + inject IMPL_CHECKLIST.txt.
#
# Unblock (run in PowerShell, NOT via the Write tool):
#   New-Item -ItemType File -Force '<repo>\.claude\.docs_read_marker'
$ErrorActionPreference = 'SilentlyContinue'
$marker = Join-Path $PSScriptRoot '..\.docs_read_marker'
$checklistPath = Join-Path $PSScriptRoot '..\IMPL_CHECKLIST.txt'

if (-not (Test-Path -LiteralPath $marker)) {
    $reason = @'
[BLOCKED by impl-gate] .claude/.docs_read_marker missing -> first edit of this turn is DENIED.
THIS turn, before editing: (1) read the dev docs for the area you touch
(START_HERE sec0, Work_Protocol 1-2/1-2a/1-2b/3, the relevant Roadmap Phase),
(2) quote the applicable rule, then (3) create the marker via PowerShell (NOT the
Write tool):
  New-Item -ItemType File -Force '<repo>\.claude\.docs_read_marker'
then retry the edit. The marker lasts the rest of this turn (all later edits pass);
it is cleared at turn end (Stop hook) and SessionStart, so the next turn re-gates once.
'@
    $payload = [ordered]@{
        hookSpecificOutput = [ordered]@{
            hookEventName            = 'PreToolUse'
            permissionDecision       = 'deny'
            permissionDecisionReason = $reason
        }
    }
    [Console]::Out.Write(($payload | ConvertTo-Json -Compress -Depth 5))
    exit 0
}

# Marker present: inject the implementation checklist (no block).
$text = ''
if (Test-Path -LiteralPath $checklistPath) {
    $text = [System.IO.File]::ReadAllText($checklistPath)
}
$payload = [ordered]@{
    hookSpecificOutput = [ordered]@{
        hookEventName     = 'PreToolUse'
        additionalContext = [string]$text
    }
}
[Console]::Out.Write(($payload | ConvertTo-Json -Compress -Depth 5))
exit 0
