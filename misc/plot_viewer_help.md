# CSV Dual-Axis Plot Viewer Pro - User Manual
## Version 3.0

---

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Interface Overview](#interface-overview)
4. [Data Tab](#data-tab)
5. [Style Tab](#style-tab)
6. [Visualization Tab](#visualization-tab)
7. [Processing Tab](#processing-tab)
8. [Analysis Tab](#analysis-tab)
9. [Plot Interaction](#plot-interaction)
10. [Menu Bar Functions](#menu-bar-functions)
11. [Toolbar](#toolbar)
12. [Dock Panels](#dock-panels)
13. [Keyboard Shortcuts](#keyboard-shortcuts)
14. [Advanced Features](#advanced-features)
15. [Troubleshooting](#troubleshooting)

---

## 1. Introduction

CSV Dual-Axis Plot Viewer Pro is a professional-grade data visualization and analysis tool designed for plotting CSV data with advanced styling, processing, and analytical capabilities. The application supports dual Y-axes, multiple file comparison, curve fitting, peak detection, and much more.

### Key Features
- Dual Y-axis plotting with independent scales
- Real-time file monitoring and auto-reload
- Advanced data processing (smoothing, decimation)
- Comprehensive analysis tools (curve fitting, peak detection, derivatives, FFT)
- Enhanced visualization (tooltips, reference lines, region highlighting)
- Multiple CSV file comparison
- Customizable styling with 16 colors, line styles, markers, and opacity control
- Multiple export formats (PNG, SVG, PDF)

### System Requirements
- Python 3.8 or higher
- Required packages: pandas, PyQt5, pyqtgraph, numpy
- Optional: scipy (for advanced smoothing and analysis features)

---

## 2. Getting Started

### Opening a CSV File

**Method 1: File Menu**
1. Click `File → Open CSV...` (or press `Ctrl+O`)
2. Navigate to your CSV file
3. Click "Open"

**Method 2: Drag and Drop**
1. Drag a CSV file from your file explorer
2. Drop it onto the application window

**Method 3: Recent Files**
1. Click `File → Recent Files`
2. Select from your 10 most recently opened files

### First Plot

1. After loading a file, select an X-axis column from the dropdown in the Data tab
2. Select one or more Y-axis columns from the "Y-Axis (Left)" list (click to select, Ctrl+click for multiple)
3. Click the "Plot Selected Columns" button in the toolbar (or press `Ctrl+P`)

Your data is now plotted!

---

## 3. Interface Overview

### Main Window Layout

The application consists of:

**Left Side: Plot Area**
- Main plotting canvas with dual Y-axes
- Interactive crosshair (toggle with `C`)
- Legend (shows in top-left corner)
- Grid overlay

**Right Side: Tabbed Control Panel**
- Data tab: Column selection and search
- Style tab: Per-series styling options
- Visualization tab: Reference lines, regions, tooltips, multi-file
- Processing tab: Smoothing and decimation
- Analysis tab: Curve fitting, peak detection, derivatives, FFT

**Bottom: Dock Panels**
- CSV Column Summary: Statistics for all numeric columns
- Dataset Statistics: Overall dataset information

**Top: Menu Bar and Toolbar**
- Quick access to common functions
- File operations, view controls, analysis tools

---

## 4. Data Tab

### Column Selection

**X-Axis Selection**
- Choose the independent variable (typically time, position, etc.)
- Single selection dropdown
- Only numeric columns are available

**Y-Axis (Left) Selection**
- Choose dependent variables for the left Y-axis
- Multiple selection allowed
- Ctrl+click to select multiple columns
- All selected series share the left axis scale

**Y-Axis (Right) Selection**
- Choose dependent variables for the right Y-axis
- Independent scale from left axis
- Useful for plotting data with different units or ranges
- Multiple selection allowed

### Column Search

Located at the top of the Data tab:
1. Type text in the "Search" box
2. Columns are filtered in real-time
3. Only matching columns are shown
4. Clear the search box to show all columns

### Quick Actions

**Select All Button**
- Selects ALL columns in both left and right Y-axis lists
- Quick way to visualize all available data

**Clear All Button**
- Deselects all columns in both lists
- Start fresh with column selection

### Recent Columns

Below the Y-axis lists, you'll see your 5 most recently used columns. This helps you quickly find commonly used data series.

### Theme Toggle

At the bottom of the Data tab:
- **Dark Theme checkbox**: Toggle between dark and light color schemes
- Dark theme: Black background, white text, optimized colors
- Light theme: White background, black text, optimized colors
- Color options automatically adjust to prevent invisible lines

---

## 5. Style Tab

The Style tab appears after you select columns and provides per-series styling controls.

### Style Options for Each Series

For every selected series, you can customize:

**Visibility**
- Toggle checkbox to show/hide the series without removing it from selection
- Useful for temporarily hiding data

**Line Style**
- Solid: Continuous line
- Dashed: Broken line with gaps
- Dotted: Small dots forming a line

**Marker Style**
- None: Line only
- o: Circular markers
- s: Square markers
- t: Triangle markers
- d: Diamond markers
- +: Plus sign markers
- x: X-shaped markers

**Color Selection**
- 16 colors available
- Dark theme: White, Red, Green, Blue, etc. (Black excluded)
- Light theme: Black, Red, Green, Blue, etc. (White excluded)
- Theme-appropriate colors ensure visibility

**Line Width**
- Range: 0.5 to 10.0 pixels
- Default: 2.0 pixels
- Adjustable in 0.5 pixel increments
- Thicker lines are easier to see, thinner for dense data

**Opacity/Alpha**
- Range: 10% to 100%
- Use slider to adjust transparency
- Lower opacity for overlapping data
- Higher opacity for primary data series

### Style Workflow

1. Select columns in Data tab
2. Switch to Style tab
3. Expand each series group
4. Adjust settings
5. Changes apply immediately when you click "Plot Selected Columns"

---

## 6. Visualization Tab

### Data Value Tooltip

**Purpose**: Display exact X,Y values near your cursor

**Usage**:
1. Check "Show tooltip on hover" (enabled by default)
2. Move mouse over the plot
3. A tooltip box appears showing:
   - X value at cursor position
   - Y values for ALL plotted series at that X position
4. Uncheck to disable tooltip

**Benefits**:
- More detailed than status bar
- Shows all series values simultaneously
- Stays near your cursor for easy reading

### Marker Size Control

**Purpose**: Adjust the size of data point markers

**Usage**:
1. Locate "Marker Size" section
2. Adjust spinner (3-30 pixels)
3. Default: 8 pixels
4. Click "Plot Selected Columns" to apply

**Tips**:
- Larger markers: Better for presentations
- Smaller markers: Better for dense data
- Size 3-5: Subtle points
- Size 15-30: Prominent markers

### Grid Spacing

**Purpose**: Control the spacing of grid lines

**Usage**:
1. Check "Auto Grid" for automatic spacing (default)
2. Uncheck for manual control
3. Set "X Spacing" value
4. Set "Y Spacing" value

**Note**: PyQtGraph has limited custom grid support. This primarily serves as a visual indicator.

### Reference Lines

**Purpose**: Add vertical or horizontal markers at specific values

**Adding a Vertical Line**:
1. Click "Add Vertical Line" button
2. Enter X value in dialog
3. Line appears in red (dashed)
4. Label shows the value

**Adding a Horizontal Line**:
1. Click "Add Horizontal Line" button
2. Enter Y value in dialog
3. Line appears in green (dashed)
4. Label shows the value

**Line Features**:
- **Draggable**: Click and drag to reposition
- **Labeled**: Shows the value automatically
- **Clear All**: Removes all reference lines

**Use Cases**:
- Mark threshold values
- Indicate target values
- Show limits or boundaries
- Compare data to specific values

### Region Highlighting

**Purpose**: Shade areas of interest with custom colors

**Adding a Region**:
1. Click "Add Shaded Region"
2. Dialog appears with options:
   - X Min: Left boundary
   - X Max: Right boundary
   - Color: Yellow, Red, Green, Blue, Cyan, Magenta
   - Opacity: 10-80%
3. Click OK

**Region Features**:
- **Movable**: Drag the entire region
- **Resizable**: Drag edges to adjust boundaries
- **Semi-transparent**: See data underneath
- **Color-coded**: Differentiate multiple regions

**Use Cases**:
- Highlight time ranges of interest
- Mark operational zones
- Indicate acceptable/unacceptable ranges
- Show phases or stages

**Clear Regions**: Removes all highlighted regions

### Multiple File Comparison

**Purpose**: Overlay data from multiple CSV files for comparison

**Adding Files**:
1. Click "Add CSV File" (or press `Ctrl+M`)
2. Select a CSV file
3. File appears in the list
4. Columns from this file now available with "(filename)" suffix

**Using Comparison Data**:
1. Added file columns appear in Data tab
2. Select columns like normal data
3. Columns show source filename in parentheses
4. Example: "Temperature (experiment2.csv)"

**Removing Files**:
1. Select file in list
2. Click "Remove Selected"
3. Associated columns disappear from selectors

**Use Cases**:
- Compare multiple experiments
- Before/after analysis
- Different sensor readings
- Historical comparisons

---

## 7. Processing Tab

### Data Smoothing

**Purpose**: Reduce noise and reveal trends

**Enabling Smoothing**:
1. Check "Enable Smoothing"
2. Select method
3. Adjust window size
4. Plot updates automatically

**Smoothing Methods**:

**Savitzky-Golay** (requires scipy)
- Polynomial fitting in sliding window
- Preserves peaks and features well
- Best for: Scientific data with features to preserve
- Window size: Odd number, typically 5-21

**Gaussian** (requires scipy)
- Gaussian-weighted average
- Very smooth results
- Best for: General noise reduction
- Window size: Larger = smoother

**Moving Average**
- Simple arithmetic mean
- Available without scipy
- Best for: Basic smoothing, any data
- Window size: Odd number recommended

**Window Size**:
- Range: 3 to 501
- Smaller: Less smoothing, preserves detail
- Larger: More smoothing, removes detail
- Must be smaller than dataset size

**Tips**:
- Start with small window (11-21)
- Increase gradually until noise is acceptable
- Too much smoothing loses real features
- Use on noisy data, not clean data

### Data Decimation

**Purpose**: Reduce data density for large datasets (improves performance)

**Usage**:
1. Check "Enable (for large datasets)"
2. Set decimation factor
3. Factor of 10: Keep every 10th point
4. Factor of 100: Keep every 100th point

**When to Use**:
- Datasets with millions of points
- Plotting is slow
- Don't need every data point
- Overviewing trends

**Caution**:
- Reduces data resolution
- May miss brief events
- Not recommended for detailed analysis
- Use with smoothing for best results

---

## 8. Analysis Tab

### Curve Fitting

**Purpose**: Fit mathematical functions to your data and display equations

**Available Fit Types**:

**Linear**: y = mx + b
- Best for: Linear relationships
- Shows: Slope and intercept

**Polynomial**: y = a₀ + a₁x + a₂x² + ... + aₙxⁿ
- Degree: 2-10 (adjustable)
- Best for: Non-linear curves
- Shows: All coefficients

**Exponential**: y = ae^(bx) + c (requires scipy)
- Best for: Growth/decay
- Shows: Amplitude, rate, offset

**Logarithmic**: y = a·ln(x) + b
- Best for: Diminishing returns
- Shows: Scale and offset
- Note: Only works with positive x values

**Power**: y = ax^b (requires scipy)
- Best for: Power law relationships
- Shows: Coefficient and exponent
- Note: Only works with positive x and y

**Performing a Fit**:
1. Check "Enable Curve Fit"
2. Select fit type
3. For Polynomial: Set degree
4. Choose series from dropdown
5. Check "Show Equation" to display on plot
6. Fit curve appears in red (dashed)

**Results Display**:
- Equation shown on plot (if enabled)
- R² value in Analysis Results panel
- R² near 1.0 = excellent fit
- R² near 0.0 = poor fit

### Peak Detection

**Purpose**: Automatically find and mark local maxima (requires scipy)

**Usage**:
1. Check "Show Peaks"
2. Set prominence threshold
3. Select series to analyze
4. Green markers appear at peaks

**Prominence Setting**:
- Range: 0.01 to 1000
- Higher value: Fewer peaks (more significant only)
- Lower value: More peaks (includes small ones)
- Adjust based on your data scale

**Results Display**:
- Peak count shown
- First 5 peak positions listed (x, y coordinates)
- Visual markers on plot (green circles)

**Use Cases**:
- Find signal peaks
- Detect oscillation maxima
- Identify reaction points
- Measure periodic events

### Derivative Analysis

**Purpose**: Calculate and visualize rate of change (dy/dx)

**Usage**:
1. Check "Show Derivative"
2. Select series to differentiate
3. Derivative appears in magenta

**Results Display**:
- Max rate of change
- Min rate of change
- Average rate of change
- Visual curve on plot

**Use Cases**:
- Find inflection points
- Measure acceleration
- Identify slope changes
- Analyze trends

**Interpretation**:
- Positive derivative: Increasing
- Negative derivative: Decreasing
- Zero derivative: Flat (local max/min)
- Large magnitude: Steep change

### FFT/Frequency Analysis

**Purpose**: Analyze frequency content of periodic data (requires scipy)

**Usage**:
1. Check "Show FFT"
2. Select series to analyze
3. Separate window opens with frequency spectrum

**FFT Window Contains**:
- Frequency spectrum plot
- Dominant frequencies listed (top 5)
- Magnitude for each frequency

**Best For**:
- Periodic/oscillating data
- Signal processing
- Vibration analysis
- Finding hidden periodicities

**Requirements**:
- Evenly spaced data works best
- Sufficient data points (100+ recommended)
- Periodic or quasi-periodic signals

**Reading Results**:
- X-axis: Frequency (Hz)
- Y-axis: Magnitude (amplitude)
- Peaks indicate dominant frequencies
- DC component (0 Hz) usually excluded

### Analysis Results Panel

At the bottom of Analysis tab, results appear as you enable features:
- Curve fit equations and R² values
- Peak counts and positions
- Derivative statistics
- Updates automatically with each analysis

---

## 9. Plot Interaction

### Mouse Controls

**Pan Mode** (default):
- **Left mouse drag**: Pan the plot
- **Right mouse drag**: Zoom box (scales both axes)
- **Mouse wheel**: Zoom in/out at cursor position

**Box Zoom Mode**:
- Click "Box Zoom" button (or toolbar)
- Left click and drag to draw zoom rectangle
- Release to zoom to that region
- Click "Box Zoom" again to return to pan mode

### Crosshair

**Enabling**: Press `C` or View → Crosshair
- Yellow dashed lines follow cursor
- X and Y coordinates shown in status bar
- Useful for reading exact positions

**Disabling**: Press `C` again

### Resetting View

**Auto-scale**: Press `R` or click "Reset Zoom"
- Returns to full data view
- Scales both axes to fit all data
- Useful after zooming too far

### Legend

The legend appears in the top-left corner:
- Shows series names
- Color-coded to match plots
- Includes left and right axis series
- Automatically updates with plot changes

---

## 10. Menu Bar Functions

### File Menu

**Open CSV... (Ctrl+O)**
- Open a new CSV file
- Replaces current data

**Recent Files**
- Submenu with last 10 files
- Click to open quickly
- Clear Recent: Remove history

**Save Plot... (Ctrl+S)**
- Export plot as image
- Formats: PNG, JPG, SVG, PDF
- High resolution (3000px width for raster)

**Export Filtered Data...**
- Save currently displayed data
- CSV or Excel format
- Includes any processing applied

**Add Comparison CSV... (Ctrl+M)**
- Load additional file for comparison
- See Visualization tab section

**Exit (Ctrl+Q)**
- Close application
- Settings are saved automatically

### View Menu

**Dark Theme**
- Toggle dark/light mode
- Synchronized with Data tab checkbox

**Crosshair**
- Toggle crosshair on/off
- Same as pressing `C`

**Column Summary**
- Show/hide CSV Column Summary dock
- Checkbox indicates visibility

**Dataset Statistics**
- Show/hide Dataset Statistics dock
- Checkbox indicates visibility

**Logarithmic X-Axis**
- Toggle log scale for X-axis
- Useful for exponential X data

**Logarithmic Y-Axis (Left)**
- Toggle log scale for left Y-axis
- Useful for exponential Y data

### Plot Menu

**Update Plot (Ctrl+P)**
- Refresh plot with current settings
- Apply style changes

**Clear Selections (Ctrl+D)**
- Deselect all Y-axis columns
- Quick way to start over

**Reset Zoom (R)**
- Auto-scale view
- Show all data

**Add Annotation... (Ctrl+A)**
- Add text label to plot
- Specify position and text

**Clear Annotations**
- Remove all text annotations

**Set Plot Title... (Ctrl+T)**
- Add custom title to plot
- Appears above plot area
- Saved with settings

**Set Axis Labels... (Ctrl+L)**
- Customize axis labels
- Override default (column names)
- Leave blank for auto labels

**Add Vertical Reference Line...**
- Add movable vertical marker
- See Visualization tab section

**Add Horizontal Reference Line...**
- Add movable horizontal marker
- See Visualization tab section

**Add Highlighted Region...**
- Add shaded area
- See Visualization tab section

### Analysis Menu

All options open the Analysis tab:

**Curve Fitting...**
- Jump to curve fitting controls

**Peak Detection...**
- Jump to peak detection controls

**Derivative...**
- Jump to derivative controls

**FFT Analysis...**
- Jump to FFT controls

### Help Menu

**Keyboard Shortcuts (F1)**
- Display all shortcuts
- Quick reference

**About**
- Version information
- Feature list
- Dependency status
- Usage tips

---

## 11. Toolbar

Quick access buttons for common operations:

**📂 Open**
- Open CSV file
- Same as File → Open

**🔍 Box Zoom**
- Toggle box zoom mode
- Checkable button

**↻ Reset Zoom**
- Auto-scale view
- Same as pressing `R`

**🗑️ Clear Selections**
- Deselect all columns
- Same as Ctrl+D

**📊 Plot**
- Update plot
- Same as Ctrl+P

**💾 Save**
- Export plot image
- Same as Ctrl+S

---

## 12. Dock Panels

### CSV Column Summary

Located at bottom of window, shows for each numeric column:
- **Column**: Name
- **Type**: Data type (int64, float64, etc.)
- **Min**: Minimum value
- **Max**: Maximum value
- **Mean**: Average value
- **Std**: Standard deviation

**Usage**:
- Quick overview of data ranges
- Identify outliers
- Check data quality
- Compare column scales

**Toggle**: View → Column Summary

### Dataset Statistics

Located at bottom of window, shows:
- **Total Rows**: Number of data points
- **Total Columns**: All columns
- **Numeric Columns**: Plottable columns
- **Memory Usage**: Dataset size in MB
- **Missing Values**: Count of NaN/null values

**Usage**:
- Dataset health check
- Performance considerations
- Data completeness

**Toggle**: View → Dataset Statistics

---

## 13. Keyboard Shortcuts

### File Operations
- `Ctrl+O`: Open CSV
- `Ctrl+S`: Save Plot
- `Ctrl+M`: Add Comparison CSV
- `Ctrl+Q`: Quit

### View Controls
- `C`: Toggle Crosshair
- `R`: Reset Zoom

### Plot Operations
- `Ctrl+P`: Update Plot
- `Ctrl+D`: Clear All Selections
- `Ctrl+T`: Set Plot Title
- `Ctrl+L`: Set Axis Labels
- `Ctrl+A`: Add Annotation

### Help
- `F1`: Show Keyboard Shortcuts

---

## 14. Advanced Features

### Auto File Monitoring

**What It Does**:
- Monitors CSV file for changes
- Automatically reloads when file modified
- Preserves zoom level after reload

**How It Works**:
- Checks file every 2 seconds
- Detects modification time changes
- Reloads data seamlessly

**Use Cases**:
- Live data logging
- Experiment monitoring
- Real-time updates

**Note**: Only monitors the primary CSV file, not comparison files

### Settings Persistence

**What's Saved**:
- Last opened file
- Theme preference (dark/light)
- Recent files list
- Column selections
- Per-series styles
- Plot title and axis labels
- Recent columns list
- Window size and position

**When Saved**:
- Automatically on changes
- When closing application

**Location**: System-specific settings storage
- Windows: Registry
- macOS: ~/Library/Preferences
- Linux: ~/.config

### Drag and Drop

**Supported**:
- CSV files only
- Single file at a time
- Drag from file explorer
- Drop anywhere on window

**Result**:
- Replaces current primary file
- Loads and plots automatically

### Multi-Series Styling

**Strategy for Many Series**:
1. Select all desired series
2. Go to Style tab
3. Use consistent line styles per axis:
   - Left axis: Solid lines
   - Right axis: Dashed lines
4. Vary colors for distinction
5. Adjust opacity if overlapping

**Performance Tips**:
- Limit to 10-15 series for clarity
- Use decimation for dense data
- Reduce marker size for many series
- Consider opacity for overlapping data

### Color Strategy

**Theme-Aware Selection**:
- Dark theme: Bright colors on dark background
- Light theme: Dark colors on light background
- Application prevents invisible selections

**Recommended Pairings**:
- Primary data: High opacity (100%), bright color
- Secondary data: Medium opacity (60-80%), contrasting color
- Reference/baseline: Low opacity (30-50%), neutral color

**Accessibility**:
- Avoid red-green only distinctions
- Use line styles to differentiate
- Include markers for clarity
- Consider colorblind-friendly palettes

---

## 15. Troubleshooting

### Performance Issues

**Problem**: Slow plotting with large datasets

**Solutions**:
1. Enable decimation (Processing tab)
   - Start with factor of 10
   - Increase if still slow
2. Reduce number of plotted series
3. Close other applications
4. Use smaller marker size
5. Disable smoothing if not needed

### Missing Features

**Problem**: Advanced smoothing unavailable

**Cause**: scipy not installed

**Solution**:
```bash
pip install scipy
```
Then restart application

**Affects**:
- Savitzky-Golay smoothing
- Gaussian smoothing
- Peak detection
- Exponential curve fitting
- Power curve fitting
- FFT analysis

### File Loading Errors

**Problem**: CSV won't load

**Possible Causes & Solutions**:

1. **Non-numeric columns**: 
   - Only numeric columns appear in selectors
   - Ensure data is numbers, not text

2. **Bad lines in CSV**:
   - Application warns but continues
   - Check console for warnings
   - Clean CSV file if needed

3. **Memory issues**:
   - File too large
   - Reduce file size
   - Use decimation after loading

4. **File permissions**:
   - Check read permissions
   - Try copying file to user directory

### Display Issues

**Problem**: Lines not visible

**Cause**: Color matches background

**Solution**: 
- Check theme setting
- Application prevents black in dark mode, white in light mode
- If using old settings, reselect color

**Problem**: Plot is blank

**Possible Causes**:
1. No columns selected → Select Y-axis columns
2. Data is NaN → Check source CSV
3. Extreme zoom → Press `R` to reset

**Problem**: Right axis not showing

**Cause**: No right axis series selected

**Solution**: Select columns in "Y-Axis (Right)" list

### Analysis Issues

**Problem**: Curve fit fails

**Possible Causes**:
1. Data not suitable for fit type
   - Try different fit type
   - Check data range (log/power need positive values)
2. Not enough data points
   - Need at least 3 points for most fits
3. scipy not installed
   - Affects exponential and power fits

**Problem**: No peaks detected

**Possible Causes**:
1. Prominence too high → Decrease threshold
2. Data has no peaks → Check data visually
3. scipy not installed → Install scipy

**Problem**: FFT window empty

**Possible Causes**:
1. scipy not installed → Install scipy
2. Non-periodic data → FFT shows noise
3. Too few points → Need 100+ points

### Comparison File Issues

**Problem**: Can't plot comparison data

**Cause**: Column name conflicts

**Solution**: Columns labeled with "(filename)" suffix
- Look for this suffix in Data tab
- Select the versioned column name

**Problem**: Comparison file disappeared

**Cause**: File removed from list

**Solution**: Re-add using "Add CSV File" button

---

## Best Practices

### Data Preparation
1. Ensure CSV has headers
2. Use numeric data for plotting
3. Remove or handle missing values
4. Keep files reasonably sized (<100MB for smooth operation)

### Plotting Workflow
1. Load file → Select X-axis → Select Y-axis(es) → Plot
2. Adjust styles in Style tab
3. Add reference lines/regions as needed
4. Apply processing if required
5. Perform analysis if needed
6. Export final plot

### Analysis Workflow
1. Plot raw data first
2. Apply smoothing if noisy
3. Identify features (peaks, trends)
4. Fit curves to understand relationships
5. Export results and plots
6. Document findings

### Performance Optimization
1. Use decimation for >100,000 points
2. Limit plotted series to essential data
3. Reduce marker size for dense data
4. Close unused dock panels
5. Restart application if sluggish

---

## Support and Resources

### Getting Help
- Press `F1` for keyboard shortcuts
- Click Help → About for feature overview
- Check this manual for detailed instructions

### Reporting Issues
- Note the exact error message
- Document steps to reproduce
- Check scipy installation status
- Provide sample data if possible

### Feature Requests
- Consider what data you need to visualize
- Think about how it fits existing workflow
- Suggest specific use cases

---

## Appendix: Example Workflows

### Example 1: Basic Time Series

**Goal**: Plot temperature over time

1. Load CSV with time and temperature columns
2. Select "time" as X-axis
3. Select "temperature" in Y-Axis (Left)
4. Click Plot
5. Add title: "Temperature vs Time"
6. Add horizontal reference line at target temp
7. Export as PNG

### Example 2: Multi-Sensor Comparison

**Goal**: Compare three temperature sensors

1. Load CSV with time and sensor1, sensor2, sensor3
2. Select "time" as X-axis
3. Select all sensor columns in Y-Axis (Left)
4. In Style tab:
   - sensor1: Red, Solid
   - sensor2: Blue, Dashed  
   - sensor3: Green, Dotted
5. Enable tooltip to read exact values
6. Add region highlighting for test period

### Example 3: Noisy Signal Analysis

**Goal**: Extract trend from noisy data

1. Load noisy data CSV
2. Plot raw signal
3. Enable Savitzky-Golay smoothing, window 21
4. Add peak detection with prominence 5.0
5. Fit polynomial (degree 3) to smoothed data
6. Export results

### Example 4: Frequency Analysis

**Goal**: Find dominant frequencies in vibration data

1. Load vibration data CSV
2. Plot time series to visualize
3. Enable FFT analysis
4. Identify top 3 frequencies
5. Document frequencies in annotations
6. Export FFT plot and main plot

### Example 5: Multi-File Comparison

**Goal**: Compare before and after data

1. Load "before.csv" as main file
2. Add "after.csv" via Ctrl+M
3. Select matching columns from both files
4. Use different colors:
   - Before data: Blue
   - After data: Red
5. Add vertical line at intervention time
6. Calculate derivatives to show rate changes
7. Export comparison plot

---

**End of Manual**

For additional assistance, refer to the About dialog (Help → About) or the keyboard shortcuts reference (F1).
