[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputWav,

    [Parameter(Mandatory = $true)]
    [string]$OutputWav,

    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$runner = Join-Path $PSScriptRoot 'SkellyVST3HostAudioTests.exe'
$plugin = Join-Path $PSScriptRoot 'Skelly Master.vst3'

if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
    throw "VST3 host renderer is missing: $runner"
}
if (-not (Test-Path -LiteralPath $plugin -PathType Container)) {
    throw "Skell Master VST3 bundle is missing: $plugin"
}

$inputPath = (Resolve-Path -LiteralPath $InputWav).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputWav)
if ([string]::Equals($inputPath, $outputPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Input and output paths must be different.'
}

$logPath = "$outputPath.render.log"
foreach ($path in @($outputPath, $logPath)) {
    if (Test-Path -LiteralPath $path) {
        if (-not $Force) {
            throw "Refusing to overwrite existing file: $path (use -Force to replace it)"
        }
        Remove-Item -LiteralPath $path -Force
    }
}

$outputDirectory = Split-Path -Parent $outputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
}

Write-Host 'Rendering through the compiled Skell Master VST3 with the SKELL MASTER reference parameter set.'
Write-Host "Input:  $inputPath"
Write-Host "Output: $outputPath"
& $runner $plugin $inputPath $outputPath 2>&1 | Tee-Object -FilePath $logPath
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "VST3 host render failed with exit code $exitCode. See $logPath"
}
if (-not (Test-Path -LiteralPath $outputPath) -or (Get-Item -LiteralPath $outputPath).Length -le 44) {
    throw "VST3 host render did not produce a valid WAV: $outputPath"
}

Write-Host "Render completed: $outputPath"
Write-Host "Render log: $logPath"
Write-Warning 'The output includes plugin latency and a short safety tail. Align it to the user-master reference before A/B. This is a JUCE-hosted VST3 render, not an FL Studio session render.'
