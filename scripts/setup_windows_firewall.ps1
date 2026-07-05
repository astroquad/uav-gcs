#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Opens the Windows Defender Firewall for Astroquad GCS UDP listeners.

.DESCRIPTION
    The GCS receives onboard telemetry on UDP 14550 and MJPEG debug video on
    UDP 5600. When the link runs over Tailscale, the Tailscale interface is
    commonly categorized under the "Public" network profile, so allow rules
    created for a home/lab LAN (Private profile) silently stop matching and
    every inbound datagram is dropped before it reaches the socket.

    This script creates idempotent inbound allow rules for both ports across
    all profiles (Domain, Private, Public). Port 5601 (discovery beacon) needs
    no inbound rule: the GCS only sends it outbound.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\setup_windows_firewall.ps1
#>

$ErrorActionPreference = "Stop"

$rules = @(
    @{ DisplayName = "Astroquad GCS Telemetry (UDP 14550 in)"; Port = 14550 },
    @{ DisplayName = "Astroquad GCS Video (UDP 5600 in)";      Port = 5600  }
)

foreach ($rule in $rules) {
    Get-NetFirewallRule -DisplayName $rule.DisplayName -ErrorAction SilentlyContinue |
        Remove-NetFirewallRule

    New-NetFirewallRule `
        -DisplayName $rule.DisplayName `
        -Direction Inbound `
        -Protocol UDP `
        -LocalPort $rule.Port `
        -Action Allow `
        -Profile Domain, Private, Public | Out-Null

    Write-Host "created: $($rule.DisplayName)"
}

Write-Host ""
Write-Host "Current Astroquad firewall rules:"
Get-NetFirewallRule -DisplayName "Astroquad GCS*" |
    Format-Table DisplayName, Enabled, Profile, Action -AutoSize

Write-Host "Verify from the onboard Pi (packets should now reach the GCS):"
Write-Host "  tailscale ping <this-laptop-tailscale-ip>"
Write-Host "  ./build/vision_debug_node --config config --line-mode light_on_dark --video"
