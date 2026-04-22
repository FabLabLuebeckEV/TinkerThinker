$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$releaseApi = "https://api.github.com/repos/FabLabLuebeckEV/TinkerThinker/releases/latest"
$outJs = Join-Path $scriptDir "webserial_release_bundle.js"

$offsets = @{
  "bootloader.bin" = "0x1000"
  "partitions.bin" = "0x8000"
  "firmware.bin"   = "0x20000"
  "littlefs.bin"   = "0x620000"
}

Write-Host "Lade Release-Metadaten..."
$release = Invoke-RestMethod -Uri $releaseApi -Headers @{ "Accept" = "application/vnd.github+json" }

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("window.TINKERTHINKER_RELEASE_BUNDLE = {")
$lines.Add("  tag_name: " + ($(ConvertTo-Json $release.tag_name -Compress)) + ",")
$lines.Add("  published_at: " + ($(ConvertTo-Json $release.published_at -Compress)) + ",")
$lines.Add('  source: "local_bundle",')
$lines.Add("  assets: {")

$first = $true
foreach ($name in @("bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin")) {
  $asset = $release.assets | Where-Object { $_.name -eq $name } | Select-Object -First 1
  if (-not $asset) {
    throw "Asset fehlt im Release: $name"
  }

  Write-Host "Lade $name..."
  $tmpFile = Join-Path $env:TEMP ("tt_bundle_" + [Guid]::NewGuid().ToString("N") + "_" + $name)
  Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $tmpFile
  $bytes = [System.IO.File]::ReadAllBytes($tmpFile)
  Remove-Item $tmpFile -Force
  $base64 = [Convert]::ToBase64String($bytes)
  $entry = '  "{0}": {{"offset": "{1}", "size": {2}, "dataBase64": "{3}"}}' -f $name, $offsets[$name], $asset.size, $base64

  if (-not $first) {
    $lines.Add(",")
  }
  $lines.Add($entry)
  $first = $false
}

$lines.Add("")
$lines.Add("  }")
$lines.Add("};")

[System.IO.File]::WriteAllLines($outJs, $lines)

Write-Host "Bundle erstellt: $outJs"
Write-Host "Tag: $($release.tag_name)"
Write-Host "webserial_installer.html kann jetzt direkt lokal geoeffnet werden."
