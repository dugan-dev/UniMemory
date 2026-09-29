param(
    [Parameter(Mandatory = $true)][string]$InputJson,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

# Optional figure export using the chart library included with Windows .NET.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Windows.Forms.DataVisualization
$data = Get-Content -LiteralPath $InputJson -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$palette = @('#94A3B8', '#0284C7', '#0D9488')
$backends = @('standard', 'mimalloc', 'jemalloc')

function New-Chart([string]$Title, [string]$Subtitle, [int]$Height) {
    $chart = New-Object System.Windows.Forms.DataVisualization.Charting.Chart
    $chart.Width = 1200
    $chart.Height = $Height
    $chart.BackColor = [System.Drawing.Color]::White
    $heading = $chart.Titles.Add($Title)
    $heading.Font = New-Object System.Drawing.Font('Segoe UI', 19)
    $heading.ForeColor = [System.Drawing.ColorTranslator]::FromHtml('#0F172A')
    $caption = $chart.Titles.Add($Subtitle)
    $caption.Font = New-Object System.Drawing.Font('Segoe UI', 12)
    $caption.ForeColor = [System.Drawing.ColorTranslator]::FromHtml('#475569')
    $legend = New-Object System.Windows.Forms.DataVisualization.Charting.Legend('Legend')
    $legend.Docking = 'Bottom'
    $legend.Alignment = 'Center'
    $legend.Font = New-Object System.Drawing.Font('Segoe UI', 13)
    $chart.Legends.Add($legend)
    return $chart
}

function New-Area($Chart, [string]$Name, [float]$Left, [float]$Width) {
    $area = New-Object System.Windows.Forms.DataVisualization.Charting.ChartArea($Name)
    $area.Position = New-Object System.Windows.Forms.DataVisualization.Charting.ElementPosition($Left, 14, $Width, 72)
    $area.AxisX.MajorGrid.Enabled = $false
    $area.AxisY.MajorGrid.LineColor = [System.Drawing.ColorTranslator]::FromHtml('#E2E8F0')
    foreach ($axis in @($area.AxisX, $area.AxisY)) {
        $axis.LabelStyle.Font = New-Object System.Drawing.Font('Segoe UI', 13)
        $axis.LineColor = [System.Drawing.ColorTranslator]::FromHtml('#CBD5E1')
        $axis.MajorTickMark.LineColor = $axis.LineColor
        $axis.TitleFont = New-Object System.Drawing.Font('Segoe UI', 13)
    }
    $area.AxisX.Interval = 1
    $Chart.ChartAreas.Add($area)
    return $area
}

$chart = New-Chart 'Allocation time by Backend' 'Standard = 1 within each workload. Shorter is faster. Statistics off.' 950
try {
    for ($panel = 0; $panel -lt 2; ++$panel) {
        $platform = @('windows', 'linux')[$panel]
        $area = New-Area $chart $platform (1 + 50 * $panel) 48
        $area.AxisX.IsReversed = $true
        $area.AxisY.Minimum = 0
        $maximumRatio = 1.0
        foreach ($row in $data.platforms.$platform.workloads) {
            foreach ($backend in $backends) {
                $maximumRatio = [Math]::Max($maximumRatio, $row.values.$backend / $row.values.standard)
            }
        }
        $area.AxisY.Maximum = [Math]::Ceiling($maximumRatio * 2) / 2 + 0.5
        $area.AxisY.Interval = 0.5
        $area.AxisY.Title = @('Windows / MSVC 19.44', 'Linux / WSL / GCC 13.3')[$panel]
        for ($index = 0; $index -lt 3; ++$index) {
            $backend = $backends[$index]
            $series = $chart.Series.Add("$platform-$backend")
            $series.ChartArea = $platform
            $series.ChartType = 'Bar'
            $series.Color = [System.Drawing.ColorTranslator]::FromHtml($palette[$index])
            $series.LegendText = @('Standard', 'mimalloc', 'jemalloc')[$index]
            $series.IsVisibleInLegend = ($panel -eq 0)
            $series.IsValueShownAsLabel = $true
            $series.LabelFormat = '0.00'
            $series.Font = New-Object System.Drawing.Font('Segoe UI', 12)
            $series['PointWidth'] = '0.85'
            foreach ($row in $data.platforms.$platform.workloads) {
                [void]$series.Points.AddXY($row.label, ($row.values.$backend / $row.values.standard))
            }
        }
    }
    $chart.SaveImage((Join-Path $OutputDirectory 'workload-comparison.png'), 'Png')
} finally { $chart.Dispose() }

$chart = New-Chart 'Memory during allocation and free' 'Windows x64. RSS above baseline, MiB. Median of 3 processes; no explicit collect.' 650
try {
    for ($panel = 0; $panel -lt 3; ++$panel) {
        $backend = $backends[$panel]
        $area = New-Area $chart $backend (1 + 33 * $panel) 32
        $area.AxisY.Minimum = 0
        $area.AxisY.Maximum = 55
        $area.AxisY.Interval = 10
        $area.AxisY.Title = @('Standard - MiB', 'mimalloc - MiB', 'jemalloc - MiB')[$panel]
        $area.AxisX.LabelStyle.Angle = -25
        $labels = @('Fill', 'Keep 1024', 'Refill', 'Keep 1024', 'Free all')
        foreach ($path in @('requests', 'native', 'disabled')) {
            $series = $chart.Series.Add("$backend-$path")
            $series.ChartArea = $backend
            $series.ChartType = if ($path -eq 'requests') { 'Area' } else { 'Line' }
            $series.LegendText = switch ($path) {
                'requests' { 'Live requests' }
                'native' { 'Native' }
                'disabled' { 'UniMemory (Statistics off)' }
            }
            $series.IsVisibleInLegend = ($panel -eq 0)
            $series.Color = if ($path -eq 'requests') { [System.Drawing.Color]::FromArgb(45, 148, 163, 184) }
                elseif ($path -eq 'native') { [System.Drawing.ColorTranslator]::FromHtml('#334155') }
                else { [System.Drawing.ColorTranslator]::FromHtml('#0284C7') }
            $series.BorderWidth = 3
            if ($path -eq 'native') { $series.BorderDashStyle = 'Dash'; $series.MarkerStyle = 'Circle'; $series.MarkerSize = 9 }
            if ($path -eq 'disabled') { $series.MarkerStyle = 'Square'; $series.MarkerSize = 6 }
            $rows = @($data.platforms.windows.pressure | Where-Object {
                $_.backend -eq $backend -and $_.path -eq $(if ($path -eq 'requests') { 'disabled' } else { $path })
            })
            for ($index = 0; $index -lt $rows.Count; ++$index) {
                $value = if ($path -eq 'requests') { $rows[$index].requested_mib } else { $rows[$index].resident_delta_mib }
                [void]$series.Points.AddXY($labels[$index], $value)
            }
        }
    }
    $chart.SaveImage((Join-Path $OutputDirectory 'memory-retention.png'), 'Png')
} finally { $chart.Dispose() }
