# PreToolUse hook (Edit|Write|MultiEdit): emit IMPL_CHECKLIST.txt as
# hookSpecificOutput.additionalContext JSON (exit 0, no blocking) so the
# implementation checklist is injected right before each source edit.
$ErrorActionPreference = 'SilentlyContinue'
$path = Join-Path $PSScriptRoot '..\IMPL_CHECKLIST.txt'
if (Test-Path -LiteralPath $path) {
    # Read as a single string via .NET (avoids PowerShell wrapping the content
    # in a FileInfo/PSObject when it flows through the pipeline -> ConvertTo-Json).
    $text = [System.IO.File]::ReadAllText($path)
    $payload = [ordered]@{
        hookSpecificOutput = [ordered]@{
            hookEventName     = 'PreToolUse'
            additionalContext = [string]$text
        }
    }
    [Console]::Out.Write(($payload | ConvertTo-Json -Compress -Depth 5))
}
exit 0
