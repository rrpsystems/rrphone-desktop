# Captura a janela do RRP Softphone (sem mexer no mouse) e monta a captura de
# tela da Microsoft Store: installer\store\<nome>.png, 1920x1080, com a janela
# centralizada sobre o fundo azul-marinho da identidade RRPBX.
#
#     .\installer\store-window.ps1 01-teclado 'Seu ramal no computador' 'Ligue, transfira e atenda direto do Windows'
#
# A Store pede no mínimo 1366x768; a janela do app é estreita (formato de
# telefone), então vai composta sobre o fundo em vez de esticada.

param([Parameter(Mandatory = $true)][string]$Name, [string]$Title = '', [string]$Subtitle = '',
      [string]$Window = '')   # título de outra janela do app, ex.: 'Configurações'
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int size);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
  public static IntPtr Find(uint pid, string title) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) { var sb = new System.Text.StringBuilder(256); GetWindowText(h, sb, 256); if (sb.ToString() == title) { found = h; return false; } } return true; }, IntPtr.Zero);
    return found;
  }
}
"@
[W]::SetProcessDPIAware() | Out-Null

$p = Get-Process RRPSoftphone | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw 'Janela do RRP Softphone não encontrada (abra o app e deixe a janela visível).' }
$h = if ($Window) { [W]::Find([uint32]$p.Id, $Window) } else { $p.MainWindowHandle }
if ($h -eq [IntPtr]::Zero) { throw "Janela '$Window' do app não encontrada." }

# Janela inteira (PrintWindow funciona mesmo atrás de outras janelas)...
$r = New-Object W+RECT
[W]::GetWindowRect($h, [ref]$r) | Out-Null
$win = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
$g = [Drawing.Graphics]::FromImage($win)
$hdc = $g.GetHdc(); [W]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc); $g.Dispose()
# ...sem a borda invisível de sombra do Windows.
$f = New-Object W+RECT
[W]::DwmGetWindowAttribute($h, 9, [ref]$f, 16) | Out-Null
$app = $win.Clone((New-Object Drawing.Rectangle ($f.L - $r.L), ($f.T - $r.T), ($f.R - $f.L), ($f.B - $f.T)), $win.PixelFormat)
$win.Dispose()

# Composição 1920x1080.
$W = 1920; $H = 1080
$bmp = New-Object Drawing.Bitmap $W, $H
$g = [Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
$top = [Drawing.ColorTranslator]::FromHtml('#0B1726'); $bottom = [Drawing.ColorTranslator]::FromHtml('#0E2436')
$g.FillRectangle((New-Object Drawing.Drawing2D.LinearGradientBrush (New-Object Drawing.Point 0, 0), (New-Object Drawing.Point $W, $H), $top, $bottom), 0, 0, $W, $H)
$glow = New-Object Drawing.Drawing2D.GraphicsPath
$glow.AddEllipse(($W / 2 - 620), -140, 1240, 1360)
$pgb = New-Object Drawing.Drawing2D.PathGradientBrush $glow
$teal = [Drawing.ColorTranslator]::FromHtml('#2DD4BF')
$pgb.CenterColor = [Drawing.Color]::FromArgb(45, $teal); $pgb.SurroundColors = @([Drawing.Color]::FromArgb(0, $teal))
$g.FillPath($pgb, $glow)

# A janela ocupa 88% da altura; com legenda, fica à direita e o texto à esquerda.
$scale = ($H * 0.88) / $app.Height
$aw = [int]($app.Width * $scale); $ah = [int]($app.Height * $scale)
$ax = if ($Title) { [int]($W * 0.66 - $aw / 2) } else { [int](($W - $aw) / 2) }
$ay = [int](($H - $ah) / 2)
# Sombra suave.
for ($i = 12; $i -ge 1; $i--) {
    $shadow = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(6, 0, 0, 0))
    $g.FillRectangle($shadow, ($ax - $i), ($ay - $i + 8), ($aw + 2 * $i), ($ah + 2 * $i))
}
$g.DrawImage($app, $ax, $ay, $aw, $ah)
$app.Dispose()

if ($Title) {
    $white = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#EEF3F7'))
    $muted = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#9FB0C2'))
    $tf = New-Object Drawing.Font 'Segoe UI Semibold', 60, ([Drawing.FontStyle]::Regular), ([Drawing.GraphicsUnit]::Pixel)
    $sf = New-Object Drawing.Font 'Segoe UI', 30, ([Drawing.FontStyle]::Regular), ([Drawing.GraphicsUnit]::Pixel)
    $tx = 150; $tw = $ax - $tx - 90
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.DrawString($Title, $tf, $white, (New-Object Drawing.RectangleF $tx, 400, $tw, 170))
    $g.FillRectangle((New-Object Drawing.SolidBrush $teal), ($tx + 4), 495, 72, 5)
    if ($Subtitle) { $g.DrawString($Subtitle, $sf, $muted, (New-Object Drawing.RectangleF ($tx + 2), 525, $tw, 140)) }
}

$dir = Join-Path $PSScriptRoot 'store'
New-Item -ItemType Directory -Force $dir | Out-Null
$out = Join-Path $dir "$Name.png"
$bmp.Save($out, [Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "$out (${W}x$H)"
