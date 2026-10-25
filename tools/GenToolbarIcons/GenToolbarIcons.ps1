# Gera os bitmaps das barras de ferramentas do SQLTools a partir dos icones Tabler (MIT, estilo outline).
#
# Uso (PowerShell 5.1, Windows):
#   .\GenToolbarIcons.ps1 -IconsDir <pasta com os .svg do Tabler> -OutDir ..\..\SQLTools\res
#
# Os SVGs (somente <path>) sao renderizados com WPF, sem dependencias externas. Cada barra gera um
# BMP de 32 bits (com alfa) com os icones lado a lado, NA MESMA ORDEM dos BUTTON de SQLTools.rc.
# Licenca dos icones: MIT (https://github.com/tabler/tabler-icons).
param(
    [Parameter(Mandatory = $true)][string]$IconsDir,
    [Parameter(Mandatory = $true)][string]$OutDir
)

Add-Type -AssemblyName PresentationCore, WindowsBase

# cores (RGB)
$Colors = @{
    slate = @(71, 84, 103)
    blue  = @(47, 111, 237)
    green = @(22, 136, 84)
    red   = @(214, 69, 65)
}

# especificacao: nome[:cor][+sobreposicao][#pilha]   ex.: "search+arrow-down", "device-floppy#", "player-play:blue"
# --- barra File (30) ---
$FileBar = @(
    'file-plus', 'folder-open', 'device-floppy', 'device-floppy#',
    'folders', 'folder-down',
    'history', 'camera',
    'layout-sidebar', 'current-location',
    'scissors', 'copy', 'clipboard',
    'arrow-back-up', 'arrow-forward-up',
    'search', 'replace', 'search+arrow-down', 'search+arrow-up', 'file-search',
    'message-2', 'message-2-off', 'letter-case-lower', 'letter-case-upper', 'brackets-contain', 'template',
    'bookmark', 'bookmark+arrow-down', 'bookmark+arrow-up', 'bookmarks-off'
)
# --- barra SQL (15) ---
$SqlBar = @(
    'player-stop:red', 'circle-check:green', 'arrow-back-up-double:red',
    'player-play:blue', 'player-skip-forward:blue', 'playlist:blue', 'terminal-2',
    'alert-triangle+arrow-down', 'alert-triangle+arrow-up',
    'database', 'hierarchy-2', 'info-circle',
    'file-text', 'chart-bar', 'variable'
)
# --- barra Connection (5; o 4o e o campo da descricao, sem icone proprio) ---
$ConnectBar = @('plug-connected:green', 'plug-connected-x:red', 'refresh-dot', 'database', 'lock')
# --- barra Booklet / resultados (22) ---
$BookletBar = @(
    'table', 'chart-line', 'hierarchy-2', 'file-text', 'history', 'braces',
    'rotate-2', 'arrows-horizontal', 'text-resize',
    'player-skip-back', 'chevrons-left', 'chevron-left', 'chevron-right', 'chevrons-right', 'player-skip-forward',
    'file-export', 'settings',
    'pin', 'pinned-off', 'world', 'file-spreadsheet', 'eraser'
)
# --- barra do popup do grid (3) ---
$GridPopupBar = @('file-text', 'code', 'text-wrap')

$Bars = @(
    @{ File = 'FileToolbar.bmp';      Size = 19; Glyph = 19; Offset = 0;  Icons = $FileBar },
    @{ File = 'SqlToolbar.bmp';       Size = 19; Glyph = 19; Offset = 0;  Icons = $SqlBar },
    @{ File = 'ConnectToolbar.bmp';   Size = 19; Glyph = 19; Offset = 0;  Icons = $ConnectBar },
    @{ File = 'BookletToolbar.bmp';   Size = 16; Glyph = 18; Offset = -1; Icons = $BookletBar },
    @{ File = 'GridPopupToolbar.bmp'; Size = 16; Glyph = 18; Offset = -1; Icons = $GridPopupBar }
)

function Get-Geometries([string]$name) {
    $path = Join-Path $IconsDir "$name.svg"
    if (-not (Test-Path $path)) { throw "Icone nao encontrado: $path" }
    [xml]$svg = Get-Content $path -Raw
    $list = @()
    foreach ($p in $svg.svg.path) {
        if ($p.stroke -eq 'none') { continue }                 # moldura 24x24 invisivel
        $list += [System.Windows.Media.Geometry]::Parse($p.d)
    }
    return , $list
}

function Draw-Icon($dc, [string]$name, [double]$scale, [double]$tx, [double]$ty, [byte[]]$rgb, [double]$alpha, [double]$stroke) {
    $color = [System.Windows.Media.Color]::FromArgb([byte]([Math]::Round(255 * $alpha)), $rgb[0], $rgb[1], $rgb[2])
    $brush = New-Object System.Windows.Media.SolidColorBrush $color
    $pen = New-Object System.Windows.Media.Pen $brush, $stroke
    $pen.StartLineCap = 'Round'; $pen.EndLineCap = 'Round'; $pen.LineJoin = 'Round'
    $dc.PushTransform((New-Object System.Windows.Media.MatrixTransform $scale, 0, 0, $scale, $tx, $ty))
    foreach ($g in (Get-Geometries $name)) { $dc.DrawGeometry($null, $pen, $g) }
    $dc.Pop()
}

function Render-Cell([string]$spec, [int]$size, [double]$glyph, [double]$offset) {
    $stack = $spec.EndsWith('#'); if ($stack) { $spec = $spec.TrimEnd('#') }
    $overlay = $null
    if ($spec.Contains('+')) { $parts = $spec.Split('+'); $spec = $parts[0]; $overlay = $parts[1] }
    $colorName = 'slate'
    if ($spec.Contains(':')) { $parts = $spec.Split(':'); $spec = $parts[0]; $colorName = $parts[1] }
    $rgb = [byte[]]$Colors[$colorName]

    $dv = New-Object System.Windows.Media.DrawingVisual
    $dc = $dv.RenderOpen()
    $s = $glyph / 24.0
    $stroke = 2.0
    if ($overlay) {
        # icone principal menor no canto superior esquerdo e uma seta pequena no canto inferior direito
        $s1 = $s * 0.80
        Draw-Icon $dc $spec $s1 $offset $offset $rgb 1.0 2.0
        $s2 = $s * 0.55
        Draw-Icon $dc $overlay $s2 ($offset + $glyph * 0.50) ($offset + $glyph * 0.50) ([byte[]]$Colors['slate']) 1.0 2.4
    }
    elseif ($stack) {
        Draw-Icon $dc $spec $s ($offset + 1.6) ($offset - 1.6) $rgb 0.45 $stroke
        Draw-Icon $dc $spec ($s * 0.92) ($offset) ($offset + 1.2) $rgb 1.0 $stroke
    }
    else {
        Draw-Icon $dc $spec $s $offset $offset $rgb 1.0 $stroke
    }
    $dc.Close()

    $rtb = New-Object System.Windows.Media.Imaging.RenderTargetBitmap $size, $size, 96, 96, ([System.Windows.Media.PixelFormats]::Pbgra32)
    $rtb.Render($dv)
    $pix = New-Object byte[] ($size * $size * 4)
    $rtb.CopyPixels($pix, $size * 4, 0)
    # alfa pre-multiplicado -> alfa direto
    for ($i = 0; $i -lt $pix.Length; $i += 4) {
        $a = $pix[$i + 3]
        if ($a -gt 0 -and $a -lt 255) {
            $pix[$i]     = [byte][Math]::Min(255, [Math]::Round($pix[$i]     * 255.0 / $a))
            $pix[$i + 1] = [byte][Math]::Min(255, [Math]::Round($pix[$i + 1] * 255.0 / $a))
            $pix[$i + 2] = [byte][Math]::Min(255, [Math]::Round($pix[$i + 2] * 255.0 / $a))
        }
    }
    return , $pix
}

function Write-Bmp32([string]$file, [int]$width, [int]$height, [byte[]]$bgra) {
    $stride = $width * 4
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms
    $imageSize = $stride * $height
    $bw.Write([byte[]][char[]]'BM')
    $bw.Write([int](14 + 40 + $imageSize)); $bw.Write([int]0); $bw.Write([int](14 + 40))
    $bw.Write([int]40); $bw.Write([int]$width); $bw.Write([int]$height)
    $bw.Write([int16]1); $bw.Write([int16]32); $bw.Write([int]0); $bw.Write([int]$imageSize)
    $bw.Write([int]2835); $bw.Write([int]2835); $bw.Write([int]0); $bw.Write([int]0)
    for ($y = $height - 1; $y -ge 0; $y--) { $bw.Write($bgra, $y * $stride, $stride) }   # BMP e de baixo para cima
    $bw.Flush()
    [System.IO.File]::WriteAllBytes($file, $ms.ToArray())
}

foreach ($bar in $Bars) {
    $n = $bar.Icons.Count
    $size = $bar.Size
    $width = $size * $n
    $all = New-Object byte[] ($width * $size * 4)
    for ($k = 0; $k -lt $n; $k++) {
        $cell = Render-Cell $bar.Icons[$k] $size $bar.Glyph $bar.Offset
        for ($y = 0; $y -lt $size; $y++) {
            [Array]::Copy($cell, $y * $size * 4, $all, ($y * $width + $k * $size) * 4, $size * 4)
        }
    }
    $target = Join-Path $OutDir $bar.File
    Write-Bmp32 $target $width $size $all
    "{0}: {1} icones, {2}x{3}" -f $bar.File, $n, $width, $size
}
