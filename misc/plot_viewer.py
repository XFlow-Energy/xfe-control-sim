import sys
import os
import numpy as np
import pandas as pd
import pyqtgraph as pg
from pathlib import Path
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QFileDialog, QVBoxLayout, QWidget, QLabel, QComboBox, QListWidget, QPushButton,
    QListWidgetItem, QHBoxLayout, QSplitter, QCheckBox, QFormLayout, QGroupBox, QTableWidget, QTableWidgetItem,
    QDockWidget, QSizePolicy, QMenuBar, QAction, QToolBar, QSpinBox, QDoubleSpinBox, QScrollArea, QMessageBox,
    QLineEdit, QDialog, QDialogButtonBox, QTabWidget, QSlider, QTextEdit, QInputDialog, QTextBrowser)
from PyQt5.QtCore import Qt, QTimer, QSettings, QVariant, QMimeData
from PyQt5.QtGui import QKeySequence, QColor, QDragEnterEvent, QDropEvent
import pyqtgraph.exporters

# Optional scipy imports with fallbacks
try:
	from scipy.signal import savgol_filter, find_peaks
	from scipy.ndimage import gaussian_filter1d
	from scipy.optimize import curve_fit
	from scipy.fft import fft, fftfreq
	SCIPY_AVAILABLE = True
except ImportError:
	SCIPY_AVAILABLE = False
	print("Warning: scipy not available. Smoothing and analysis features will be limited.")

# Optional markdown rendering
try:
	import markdown
	MARKDOWN_AVAILABLE = True
except ImportError:
	MARKDOWN_AVAILABLE = False
	print(
	    "Warning: markdown library not available. Help will display as plain text. Install with: pip install markdown")

class CSVPlotter(QMainWindow):

	def __init__(self):
		super().__init__()
		self.setWindowTitle("CSV Dual-Axis Plot Viewer Pro")
		self.resize(1400, 900)
		self.setAcceptDrops(True)

		self.settings = QSettings("XFlow", "CSVPlotter")
		self.theme_dark = self.settings.value("theme_dark", True, type=bool)
		self.df = None
		self.csv_path = None
		self.csv_mtime = None
		self.zoom_mode = False
		self.series_saved_styles = self.settings.value("series_styles", {}, type=dict)
		self.series_visibility = {}
		self.crosshair_enabled = False
		self.recent_files = self.settings.value("recent_files", [], type=list)[:10]
		self.recent_columns = self.settings.value("recent_columns", [], type=list)[:20]
		self.plot_title = self.settings.value("plot_title", "", type=str)
		self.x_axis_label = self.settings.value("x_axis_label", "", type=str)
		self.y1_axis_label = self.settings.value("y1_axis_label", "", type=str)
		self.y2_axis_label = self.settings.value("y2_axis_label", "", type=str)
		self.analysis_items = []
		self.loaded_files = {}  # Store multiple CSV files
		self.reference_lines = []  # Store reference lines
		self.highlighted_regions = []  # Store highlighted regions
		self.data_tooltip = None  # Tooltip for showing data values

		self.setup_ui()
		self.create_menu_bar()
		self.create_toolbar()
		self.init_csv_preview_dock()
		self.init_statistics_dock()
		self.replace_plot_widget()

		self.monitor_timer = QTimer(self)
		self.monitor_timer.setInterval(2000)
		self.monitor_timer.timeout.connect(self.check_file_update)
		self.monitor_timer.start()

		last_file = self.settings.value("last_csv_file", "", type=str)
		if last_file and os.path.isfile(last_file):
			self.load_csv(last_file)

	def setup_ui(self):
		self.central_widget = QWidget()
		self.setCentralWidget(self.central_widget)
		self.main_layout = QVBoxLayout(self.central_widget)

		self.splitter = QSplitter(Qt.Horizontal)
		self.main_layout.addWidget(self.splitter)

		self.plot_area = QWidget()
		self.splitter.addWidget(self.plot_area)

		# Control panel with tabs
		self.control_panel = QWidget()
		self.control_layout = QVBoxLayout(self.control_panel)
		self.splitter.addWidget(self.control_panel)

		# Theme toggle at top, outside tabs
		theme_layout = QHBoxLayout()
		self.theme_checkbox = QCheckBox("Dark Theme")
		self.theme_checkbox.setChecked(self.theme_dark)
		self.theme_checkbox.stateChanged.connect(self.toggle_theme_checkbox)
		theme_layout.addWidget(self.theme_checkbox)
		theme_layout.addStretch()
		self.control_layout.addLayout(theme_layout)

		self.tabs = QTabWidget()
		self.control_layout.addWidget(self.tabs)

		# Data Selection Tab
		self.data_tab = QWidget()
		self.data_layout = QVBoxLayout(self.data_tab)
		self.build_data_controls()
		self.tabs.addTab(self.data_tab, "Data")

		# Style Tab
		self.style_tab = QWidget()
		self.style_layout = QVBoxLayout(self.style_tab)
		self.build_style_controls()
		self.tabs.addTab(self.style_tab, "Style")

		# Visualization Tab
		self.viz_tab = QWidget()
		self.viz_layout = QVBoxLayout(self.viz_tab)
		self.build_visualization_controls()
		self.tabs.addTab(self.viz_tab, "Visualization")

		# Processing Tab
		self.processing_tab = QWidget()
		self.processing_layout = QVBoxLayout(self.processing_tab)
		self.build_processing_controls()
		self.tabs.addTab(self.processing_tab, "Processing")

		# Analysis Tab
		self.analysis_tab = QWidget()
		self.analysis_layout = QVBoxLayout(self.analysis_tab)
		self.build_analysis_controls()
		self.tabs.addTab(self.analysis_tab, "Analysis")

		# Help Tab
		self.help_tab = QWidget()
		self.help_layout = QVBoxLayout(self.help_tab)
		self.build_help_tab()
		self.tabs.addTab(self.help_tab, "Help")

		self.splitter.setStretchFactor(0, 3)
		self.splitter.setStretchFactor(1, 1)

	def toggle_tooltip(self):
		"""Toggle data value tooltip on/off"""
		if self.tooltip_enabled.isChecked():
			if self.data_tooltip is None:
				self.data_tooltip = pg.TextItem(anchor=(0, 1), color='y')
				self.main_plot.addItem(self.data_tooltip)
		else:
			if self.data_tooltip is not None:
				self.main_plot.removeItem(self.data_tooltip)
				self.data_tooltip = None

	def find_nearest_point(self, x_pos):
		"""Find the nearest data point to the given x position"""
		if self.df is None or not self.x_selector.currentText():
			return None

		x_col = self.x_selector.currentText()
		x_data = self.df[x_col].to_numpy()

		# Find nearest x index
		idx = np.argmin(np.abs(x_data - x_pos))

		# Collect y values at this x position
		info = {x_col: x_data[idx]}

		# Get selected y1 columns
		for item in self.y1_list.selectedItems():
			y_col = item.text()
			if y_col in self.df.columns:
				info[y_col] = self.df[y_col].iloc[idx]

		# Get selected y2 columns
		for item in self.y2_list.selectedItems():
			y_col = item.text()
			if y_col in self.df.columns:
				info[y_col] = self.df[y_col].iloc[idx]

		return x_data[idx], info

	# ============================================================================
	# GRID SPACING IMPLEMENTATION
	# ============================================================================

	def update_grid(self):
		"""Update grid spacing based on user settings"""
		if not hasattr(self, 'main_plot'):
			return

		if self.grid_auto.isChecked():
			# Auto grid - use default pyqtgraph behavior
			self.main_plot.showGrid(x=True, y=True, alpha=0.3)
		else:
			# Custom grid spacing
			x_spacing = self.grid_x_spacing.value()
			y_spacing = self.grid_y_spacing.value()

			# Get axis objects
			ax_x = self.main_plot.getAxis('bottom')
			ax_y = self.main_plot.getAxis('left')

			# Set tick spacing (this is a workaround as pyqtgraph doesn't have direct grid spacing)
			# We'll force specific tick values
			if self.df is not None and self.x_selector.currentText():
				x_col = self.x_selector.currentText()
				x_data = self.df[x_col].to_numpy()
				x_min, x_max = x_data.min(), x_data.max()
				x_ticks = np.arange(
				    np.floor(x_min / x_spacing) * x_spacing,
				    np.ceil(x_max / x_spacing) * x_spacing + x_spacing, x_spacing)
				ax_x.setTicks([[(v, str(v)) for v in x_ticks]])

			self.main_plot.showGrid(x=True, y=True, alpha=0.3)

	# ============================================================================
	# REFERENCE LINES IMPLEMENTATION
	# ============================================================================

	def add_vertical_reference(self):
		"""Add a vertical reference line"""
		if self.df is None or not self.x_selector.currentText():
			QMessageBox.warning(self, "No Data", "Load data first before adding reference lines.")
			return

		x_col = self.x_selector.currentText()
		x_data = self.df[x_col].to_numpy()
		x_mid = (x_data.min() + x_data.max()) / 2

		value, ok = QInputDialog.getDouble(
		    self, "Vertical Reference Line", f"Enter X position:", x_mid, x_data.min(), x_data.max(), 4)

		if ok:
			line = pg.InfiniteLine(
			    pos=value,
			    angle=90,
			    movable=True,
			    pen=pg.mkPen('r', width=2, style=Qt.DashLine),
			    label=f'x={value:.2f}',
			    labelOpts={
			        'position': 0.95,
			        'color': 'r'
			    })
			self.main_plot.addItem(line)
			self.reference_lines.append(line)
			self.statusBar().showMessage(f"Added vertical line at x={value:.2f}")

	def add_horizontal_reference(self):
		"""Add a horizontal reference line"""
		if self.df is None:
			QMessageBox.warning(self, "No Data", "Load data first before adding reference lines.")
			return

		# Get approximate y range from selected columns
		y_min, y_max = float('inf'), float('-inf')
		for item in self.y1_list.selectedItems():
			y_col = item.text()
			if y_col in self.df.columns:
				y_min = min(y_min, self.df[y_col].min())
				y_max = max(y_max, self.df[y_col].max())

		if y_min == float('inf'):
			y_min, y_max = 0, 100

		y_mid = (y_min + y_max) / 2

		value, ok = QInputDialog.getDouble(
		    self, "Horizontal Reference Line", f"Enter Y position:", y_mid, y_min * 10, y_max * 10, 4)

		if ok:
			line = pg.InfiniteLine(
			    pos=value,
			    angle=0,
			    movable=True,
			    pen=pg.mkPen('g', width=2, style=Qt.DashLine),
			    label=f'y={value:.2f}',
			    labelOpts={
			        'position': 0.95,
			        'color': 'g'
			    })
			self.main_plot.addItem(line)
			self.reference_lines.append(line)
			self.statusBar().showMessage(f"Added horizontal line at y={value:.2f}")

	def clear_reference_lines(self):
		"""Remove all reference lines"""
		for line in self.reference_lines:
			self.main_plot.removeItem(line)
		self.reference_lines.clear()
		self.statusBar().showMessage("Cleared all reference lines")

	# ============================================================================
	# REGION HIGHLIGHTING IMPLEMENTATION
	# ============================================================================

	def add_highlighted_region(self):
		"""Add a shaded vertical region"""
		if self.df is None or not self.x_selector.currentText():
			QMessageBox.warning(self, "No Data", "Load data first before adding regions.")
			return

		x_col = self.x_selector.currentText()
		x_data = self.df[x_col].to_numpy()
		x_min, x_max = x_data.min(), x_data.max()
		x_range = x_max - x_min

		# Dialog for region parameters
		dialog = QDialog(self)
		dialog.setWindowTitle("Add Highlighted Region")
		layout = QFormLayout(dialog)

		start_input = QDoubleSpinBox()
		start_input.setRange(x_min - x_range, x_max + x_range)
		start_input.setValue(x_min + x_range * 0.3)
		start_input.setDecimals(4)

		end_input = QDoubleSpinBox()
		end_input.setRange(x_min - x_range, x_max + x_range)
		end_input.setValue(x_min + x_range * 0.7)
		end_input.setDecimals(4)

		color_combo = QComboBox()
		colors = ['Yellow', 'Red', 'Green', 'Blue', 'Cyan', 'Magenta', 'Gray']
		color_combo.addItems(colors)

		alpha_slider = QSlider(Qt.Horizontal)
		alpha_slider.setRange(10, 80)
		alpha_slider.setValue(30)
		alpha_label = QLabel("30%")
		alpha_slider.valueChanged.connect(lambda v: alpha_label.setText(f"{v}%"))

		layout.addRow("Start X:", start_input)
		layout.addRow("End X:", end_input)
		layout.addRow("Color:", color_combo)
		alpha_layout = QHBoxLayout()
		alpha_layout.addWidget(alpha_slider)
		alpha_layout.addWidget(alpha_label)
		layout.addRow("Opacity:", alpha_layout)

		buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
		buttons.accepted.connect(dialog.accept)
		buttons.rejected.connect(dialog.reject)
		layout.addRow(buttons)

		if dialog.exec_() == QDialog.Accepted:
			start_x = start_input.value()
			end_x = end_input.value()
			color_name = color_combo.currentText()
			alpha = alpha_slider.value()

			color_map = {
			    'Yellow': (255, 255, 0),
			    'Red': (255, 0, 0),
			    'Green': (0, 255, 0),
			    'Blue': (0, 0, 255),
			    'Cyan': (0, 255, 255),
			    'Magenta': (255, 0, 255),
			    'Gray': (128, 128, 128)
			}

			color = color_map.get(color_name, (255, 255, 0))
			region = pg.LinearRegionItem(values=(start_x, end_x), brush=(*color, int(255 * alpha / 100)), movable=True)
			self.main_plot.addItem(region)
			self.highlighted_regions.append(region)
			self.statusBar().showMessage(f"Added region from {start_x:.2f} to {end_x:.2f}")

	def clear_regions(self):
		"""Remove all highlighted regions"""
		for region in self.highlighted_regions:
			self.main_plot.removeItem(region)
		self.highlighted_regions.clear()
		self.statusBar().showMessage("Cleared all highlighted regions")

	# ============================================================================
	# MULTIPLE FILE COMPARISON IMPLEMENTATION
	# ============================================================================

	def add_comparison_file(self):
		"""Add another CSV file for comparison"""
		start_dir = self.settings.value("last_csv_dir", os.path.expanduser("~"))
		filename, _ = QFileDialog.getOpenFileName(
		    self, "Add Comparison CSV", start_dir, "CSV Files (*.csv);;All Files (*)")

		if filename:
			try:
				df = pd.read_csv(filename, on_bad_lines='warn')
				file_name = os.path.basename(filename)

				# Avoid duplicate names
				base_name = file_name
				counter = 1
				while file_name in self.loaded_files:
					file_name = f"{base_name} ({counter})"
					counter += 1

				self.loaded_files[file_name] = df
				self.file_list.addItem(file_name)
				self.statusBar().showMessage(f"Added comparison file: {file_name}")

				# Reload column selectors to include new file's columns
				self.load_column_selectors()

			except Exception as e:
				QMessageBox.critical(self, "Error", f"Failed to load comparison file: {e}")

	def remove_comparison_file(self):
		"""Remove selected comparison file"""
		current_item = self.file_list.currentItem()
		if current_item:
			file_name = current_item.text()
			if file_name in self.loaded_files:
				del self.loaded_files[file_name]
				self.file_list.takeItem(self.file_list.row(current_item))
				self.statusBar().showMessage(f"Removed: {file_name}")
				self.load_column_selectors()

	# ============================================================================
	# ANALYSIS FEATURES IMPLEMENTATION
	# ============================================================================

	def update_analysis(self):
		"""Update all analysis overlays"""
		if self.df is None:
			return

		# Clear previous analysis items
		for item in self.analysis_items:
			if item in self.main_plot.items:
				self.main_plot.removeItem(item)
		self.analysis_items.clear()

		analysis_text = ""

		# Curve Fitting
		if self.fit_enabled.isChecked() and self.fit_series.currentIndex() > 0:
			result = self.perform_curve_fit()
			if result:
				analysis_text += result + "\n\n"

		# Peak Detection
		if self.peaks_enabled.isChecked() and self.peak_series.currentIndex() > 0:
			result = self.perform_peak_detection()
			if result:
				analysis_text += result + "\n\n"

		# Derivative
		if self.deriv_enabled.isChecked() and self.deriv_series.currentIndex() > 0:
			result = self.perform_derivative()
			if result:
				analysis_text += result + "\n\n"

		# Update analysis results text
		if analysis_text:
			self.analysis_results.setPlainText(analysis_text.strip())
		else:
			self.analysis_results.setPlainText("No analysis performed. Enable features and select series above.")

	def on_fit_type_changed(self):
		"""Show/hide polynomial degree selector and custom equation input based on fit type"""
		fit_type = self.fit_type.currentText()

		if fit_type == "Polynomial":
			self.poly_degree.show()
			self.poly_degree_label.show()
			self.custom_equation_input.hide()
			self.custom_equation_label.hide()
			self.custom_params_input.hide()
			self.custom_params_label.hide()
			self.equation_help_btn.hide()
		elif fit_type == "Custom Equation":
			self.poly_degree.hide()
			self.poly_degree_label.hide()
			self.custom_equation_input.show()
			self.custom_equation_label.show()
			self.custom_params_input.show()
			self.custom_params_label.show()
			self.equation_help_btn.show()
		else:
			self.poly_degree.hide()
			self.poly_degree_label.hide()
			self.custom_equation_input.hide()
			self.custom_equation_label.hide()
			self.custom_params_input.hide()
			self.custom_params_label.hide()
			self.equation_help_btn.hide()

		self.update_analysis()

	def show_equation_help(self):
		"""Show help dialog for custom equation syntax"""
		help_text = """
<h3>Custom Equation Syntax</h3>

<p><b>How to write equations:</b></p>
<ul>
<li>Use <code>x</code> as the independent variable</li>
<li>Use parameter names like <code>a</code>, <code>b</code>, <code>c</code>, etc.</li>
<li>Available functions: <code>exp()</code>, <code>log()</code>, <code>sin()</code>, <code>cos()</code>, <code>tan()</code>, <code>sqrt()</code>, <code>abs()</code></li>
<li>Operators: <code>+</code>, <code>-</code>, <code>*</code>, <code>/</code>, <code>**</code> (power)</li>
</ul>

<p><b>Examples:</b></p>
<ul>
<li>Exponential decay: <code>a * exp(-b * x)</code></li>
<li>Gaussian: <code>a * exp(-((x - b)**2) / (2 * c**2))</code></li>
<li>Sine wave: <code>a * sin(b * x + c) + d</code></li>
<li>Logistic: <code>a / (1 + exp(-b * (x - c)))</code></li>
<li>Rational: <code>(a * x + b) / (c * x + d)</code></li>
<li>Double exponential: <code>a * exp(-b * x) + c * exp(-d * x)</code></li>
</ul>

<p><b>Parameters:</b></p>
<p>Enter initial guesses as comma-separated values in the order they appear in your equation.</p>
<p>Example: For equation <code>a * exp(-b * x)</code>, enter: <code>1.0, 0.1</code></p>

<p><b>Tips:</b></p>
<ul>
<li>Good initial guesses help the fitting converge</li>
<li>Use parentheses to control order of operations</li>
<li>Avoid division by zero or other mathematical errors</li>
</ul>
"""
		QMessageBox.information(self, "Custom Equation Help", help_text)

	def perform_curve_fit(self):
		"""Perform curve fitting on selected series"""
		if not SCIPY_AVAILABLE:
			return "Curve fitting requires scipy. Install with: pip install scipy"

		series_name = self.fit_series.currentText()
		if series_name not in self.df.columns:
			return None

		x_col = self.x_selector.currentText()
		x = self.df[x_col].to_numpy()
		y = self.df[series_name].to_numpy()

		# Remove NaN values
		mask = ~(np.isnan(x) | np.isnan(y))
		x = x[mask]
		y = y[mask]

		if len(x) < 2:
			return "Insufficient data for curve fitting"

		fit_type = self.fit_type.currentText()

		try:
			if fit_type == "Linear":
				coeffs = np.polyfit(x, y, 1)
				y_fit = np.polyval(coeffs, x)
				equation = f"y = {coeffs[0]:.4g}x + {coeffs[1]:.4g}"

			elif fit_type == "Polynomial":
				degree = self.poly_degree.value()
				coeffs = np.polyfit(x, y, degree)
				y_fit = np.polyval(coeffs, x)
				terms = []
				for i, c in enumerate(coeffs):
					power = len(coeffs) - 1 - i
					if power == 0:
						terms.append(f"{c:.4g}")
					elif power == 1:
						terms.append(f"{c:.4g}x")
					else:
						terms.append(f"{c:.4g}x^{power}")
				equation = "y = " + " + ".join(terms)

			elif fit_type == "Exponential":
				# y = a * exp(b * x)
				def exp_func(x, a, b):
					return a * np.exp(b * x)

				popt, _ = curve_fit(exp_func, x, y, maxfev=5000)
				y_fit = exp_func(x, *popt)
				equation = f"y = {popt[0]:.4g} * exp({popt[1]:.4g} * x)"

			elif fit_type == "Logarithmic":
				# y = a * log(x) + b
				def log_func(x, a, b):
					return a * np.log(x) + b

				popt, _ = curve_fit(log_func, x, y, maxfev=5000)
				y_fit = log_func(x, *popt)
				equation = f"y = {popt[0]:.4g} * ln(x) + {popt[1]:.4g}"

			elif fit_type == "Power":
				# y = a * x^b
				def power_func(x, a, b):
					return a * np.power(x, b)

				popt, _ = curve_fit(power_func, x, y, maxfev=5000)
				y_fit = power_func(x, *popt)
				equation = f"y = {popt[0]:.4g} * x^{popt[1]:.4g}"

			elif fit_type == "Custom Equation":
				# Parse custom equation
				equation_str = self.custom_equation_input.text().strip()
				params_str = self.custom_params_input.text().strip()

				if not equation_str:
					return "Please enter a custom equation"

				# Parse initial parameters
				if params_str:
					try:
						initial_params = [float(p.strip()) for p in params_str.split(',')]
					except ValueError:
						return "Invalid parameter format. Use comma-separated numbers (e.g., 1.0, 0.1, 2.0)"
				else:
					# Default initial guess
					initial_params = [1.0] * 3  # Assume up to 3 parameters

				# Create function from equation string
				try:
					# Extract parameter names from equation (a, b, c, etc.)
					import re
					param_pattern = r'\b([a-z])\b'
					params_in_eq = sorted(set(re.findall(param_pattern, equation_str.lower())) - {'x', 'e'})

					if not params_in_eq:
						return "No parameters found in equation. Use a, b, c, etc."

					# Adjust initial_params to match number of parameters
					if len(initial_params) < len(params_in_eq):
						initial_params.extend([1.0] * (len(params_in_eq) - len(initial_params)))
					elif len(initial_params) > len(params_in_eq):
						initial_params = initial_params[:len(params_in_eq)]

					# Create function signature
					func_params = ', '.join(params_in_eq)

					# Build safe namespace for eval
					safe_namespace = {
					    'exp': np.exp,
					    'log': np.log,
					    'ln': np.log,
					    'sin': np.sin,
					    'cos': np.cos,
					    'tan': np.tan,
					    'sqrt': np.sqrt,
					    'abs': np.abs,
					    'pi': np.pi,
					    'e': np.e,
					    'np': np
					}

					# Create the fitting function
					def custom_func(x, *params):
						local_vars = safe_namespace.copy()
						local_vars['x'] = x
						for param_name, param_val in zip(params_in_eq, params):
							local_vars[param_name] = param_val
						return eval(equation_str, {"__builtins__": {}}, local_vars)

					# Perform fit
					popt, pcov = curve_fit(custom_func, x, y, p0=initial_params, maxfev=10000)
					y_fit = custom_func(x, *popt)

					# Build equation string with fitted parameters
					fitted_eq = equation_str
					for param_name, param_val in zip(params_in_eq, popt):
						fitted_eq = fitted_eq.replace(param_name, f"{param_val:.4g}")
					equation = f"y = {fitted_eq}"

					# Add parameter values to output
					param_str = ", ".join([f"{p}={v:.4g}" for p, v in zip(params_in_eq, popt)])
					equation += f"\nParameters: {param_str}"

				except SyntaxError as e:
					return f"Equation syntax error: {str(e)}"
				except Exception as e:
					return f"Custom equation fit failed: {str(e)}\nCheck your equation syntax and initial parameters."

			# Calculate R²
			ss_res = np.sum((y - y_fit)**2)
			ss_tot = np.sum((y - np.mean(y))**2)
			r_squared = 1 - (ss_res / ss_tot) if ss_tot != 0 else 0

			# Plot the fit
			fit_curve = self.main_plot.plot(
			    x, y_fit, pen=pg.mkPen('r', width=3, style=Qt.DashLine), name=f'{series_name} Fit')
			self.analysis_items.append(fit_curve)

			# Add equation as text if enabled
			if self.show_equation.isChecked():
				text_item = pg.TextItem(f"{equation}\nR² = {r_squared:.4f}", anchor=(0, 1), color='r')
				text_item.setPos(x[len(x) // 2], y_fit[len(y_fit) // 2])
				self.main_plot.addItem(text_item)
				self.analysis_items.append(text_item)

			return f"CURVE FIT ({series_name}):\n{equation}\nR² = {r_squared:.4f}"

		except Exception as e:
			return f"Curve fitting failed: {str(e)}"

	def perform_peak_detection(self):
		"""Detect and mark peaks in selected series"""
		if not SCIPY_AVAILABLE:
			return "Peak detection requires scipy. Install with: pip install scipy"

		series_name = self.peak_series.currentText()
		if series_name not in self.df.columns:
			return None

		x_col = self.x_selector.currentText()
		x = self.df[x_col].to_numpy()
		y = self.df[series_name].to_numpy()

		prominence = self.peak_prominence.value()
		peaks, properties = find_peaks(y, prominence=prominence)

		if len(peaks) == 0:
			return f"PEAK DETECTION ({series_name}):\nNo peaks found with prominence >= {prominence}"

		# Plot peaks
		scatter = pg.ScatterPlotItem(
		    x=x[peaks], y=y[peaks], symbol='o', size=15, pen=pg.mkPen('r', width=2), brush=pg.mkBrush(255, 0, 0, 120))
		self.main_plot.addItem(scatter)
		self.analysis_items.append(scatter)

		# Format peak info
		peak_info = f"PEAK DETECTION ({series_name}):\nFound {len(peaks)} peaks\n"
		for i, (px, py) in enumerate(zip(x[peaks], y[peaks])):
			peak_info += f"Peak {i+1}: x={px:.4g}, y={py:.4g}\n"
			if i >= 9:  # Limit to first 10 peaks
				peak_info += f"... and {len(peaks) - 10} more\n"
				break

		return peak_info

	def perform_derivative(self):
		"""Calculate and plot derivative of selected series"""
		series_name = self.deriv_series.currentText()
		if series_name not in self.df.columns:
			return None

		x_col = self.x_selector.currentText()
		x = self.df[x_col].to_numpy()
		y = self.df[series_name].to_numpy()

		# Calculate derivative (dy/dx)
		dy = np.gradient(y, x)

		# Plot derivative
		deriv_curve = self.main_plot.plot(
		    x, dy, pen=pg.mkPen('m', width=2, style=Qt.DotLine), name=f'd({series_name})/dx')
		self.analysis_items.append(deriv_curve)

		# Statistics
		deriv_mean = np.mean(dy)
		deriv_max = np.max(dy)
		deriv_min = np.min(dy)

		return f"DERIVATIVE ({series_name}):\nMean: {deriv_mean:.4g}\nMax: {deriv_max:.4g}\nMin: {deriv_min:.4g}"

	def show_fft_window(self):
		"""Show FFT analysis in a separate window"""
		if not self.fft_enabled.isChecked():
			return

		if not SCIPY_AVAILABLE:
			QMessageBox.warning(self, "FFT Error", "FFT requires scipy. Install with: pip install scipy")
			self.fft_enabled.setChecked(False)
			return

		series_name = self.fft_series.currentText()
		if series_name not in self.df.columns:
			QMessageBox.warning(self, "FFT Error", "Select a series for FFT analysis")
			self.fft_enabled.setChecked(False)
			return

		x_col = self.x_selector.currentText()
		x = self.df[x_col].to_numpy()
		y = self.df[series_name].to_numpy()

		# Calculate FFT
		N = len(y)
		T = (x[-1] - x[0]) / (N - 1)  # Sampling interval
		yf = fft(y)
		xf = fftfreq(N, T)[:N // 2]

		# Create FFT window
		fft_window = QDialog(self)
		fft_window.setWindowTitle(f"FFT Analysis: {series_name}")
		fft_window.resize(800, 600)

		layout = QVBoxLayout(fft_window)

		# Create plot
		plot_widget = pg.PlotWidget()
		plot_widget.setLabel('left', 'Magnitude')
		plot_widget.setLabel('bottom', 'Frequency')
		plot_widget.showGrid(x=True, y=True, alpha=0.3)

		# Plot magnitude spectrum
		magnitude = 2.0 / N * np.abs(yf[0:N // 2])
		plot_widget.plot(xf, magnitude, pen='b')

		layout.addWidget(plot_widget)

		# Info label
		peak_freq_idx = np.argmax(magnitude[1:]) + 1  # Skip DC component
		peak_freq = xf[peak_freq_idx]
		info_label = QLabel(f"Dominant Frequency: {peak_freq:.4g} Hz")
		layout.addWidget(info_label)

		# Close button
		close_btn = QPushButton("Close")
		close_btn.clicked.connect(fft_window.close)
		layout.addWidget(close_btn)

		fft_window.exec_()

	def build_data_controls(self):
		# Column search/filter
		search_layout = QHBoxLayout()
		search_layout.addWidget(QLabel("🔍 Search:"))
		self.column_search = QLineEdit()
		self.column_search.setPlaceholderText("Filter columns...")
		self.column_search.textChanged.connect(self.filter_columns)
		search_layout.addWidget(self.column_search)
		self.data_layout.addLayout(search_layout)

		self.data_layout.addWidget(QLabel("<b>X-Axis:</b>"))
		self.x_selector = QComboBox()
		self.x_selector.currentTextChanged.connect(self.update_recent_columns)
		self.data_layout.addWidget(self.x_selector)

		self.data_layout.addWidget(QLabel("<b>Y-Axis (Left):</b>"))
		self.y1_list = QListWidget()
		self.y1_list.setSelectionMode(QListWidget.MultiSelection)
		self.y1_list.itemSelectionChanged.connect(self.update_style_selectors)
		self.y1_list.itemSelectionChanged.connect(self.update_recent_columns)
		self.data_layout.addWidget(self.y1_list)

		self.data_layout.addWidget(QLabel("<b>Y-Axis (Right):</b>"))
		self.y2_list = QListWidget()
		self.y2_list.setSelectionMode(QListWidget.MultiSelection)
		self.y2_list.itemSelectionChanged.connect(self.update_style_selectors)
		self.y2_list.itemSelectionChanged.connect(self.update_recent_columns)
		self.data_layout.addWidget(self.y2_list)

		# Recent columns quick access
		if self.recent_columns:
			self.data_layout.addWidget(QLabel("<b>Recent Columns:</b>"))
			self.recent_label = QLabel(", ".join(self.recent_columns[:5]))
			self.recent_label.setWordWrap(True)
			self.recent_label.setStyleSheet("font-size: 9pt; font-style: italic;")
			self.data_layout.addWidget(self.recent_label)

		# Quick action buttons
		btn_layout = QHBoxLayout()
		self.select_all_btn = QPushButton("Select All")
		self.select_all_btn.clicked.connect(self.select_all_columns)
		self.clear_all_btn = QPushButton("Clear All")
		self.clear_all_btn.clicked.connect(self.clear_all_selections)
		btn_layout.addWidget(self.select_all_btn)
		btn_layout.addWidget(self.clear_all_btn)
		self.data_layout.addLayout(btn_layout)

	def build_style_controls(self):
		self.style_scroll = QScrollArea()
		self.style_scroll.setWidgetResizable(True)
		self.style_widget = QWidget()
		self.style_form = QFormLayout(self.style_widget)
		self.style_scroll.setWidget(self.style_widget)
		self.style_layout.addWidget(self.style_scroll)

	def build_visualization_controls(self):
		self.viz_layout.addWidget(QLabel("<b>Visualization Options:</b>"))

		# Data Tooltip
		tooltip_group = QGroupBox("Data Value Tooltip")
		tooltip_layout = QFormLayout()
		self.tooltip_enabled = QCheckBox("Show tooltip on hover")
		self.tooltip_enabled.setChecked(True)
		self.tooltip_enabled.stateChanged.connect(
		    lambda: self.toggle_tooltip() if hasattr(self, 'toggle_tooltip') else None)
		tooltip_layout.addRow(self.tooltip_enabled)
		tooltip_group.setLayout(tooltip_layout)
		self.viz_layout.addWidget(tooltip_group)

		# Marker Size Control
		marker_group = QGroupBox("Marker Size")
		marker_layout = QFormLayout()
		self.marker_size = QSpinBox()
		self.marker_size.setRange(3, 30)
		self.marker_size.setValue(8)
		self.marker_size.valueChanged.connect(lambda: self.plot_selected() if hasattr(self, 'plot_selected') else None)
		marker_layout.addRow("Size (px):", self.marker_size)
		marker_group.setLayout(marker_layout)
		self.viz_layout.addWidget(marker_group)

		# Grid Spacing
		grid_group = QGroupBox("Grid Spacing")
		grid_layout = QFormLayout()
		self.grid_auto = QCheckBox("Auto Grid")
		self.grid_auto.setChecked(True)
		self.grid_auto.stateChanged.connect(self.update_grid)
		grid_layout.addRow(self.grid_auto)

		self.grid_x_spacing = QDoubleSpinBox()
		self.grid_x_spacing.setRange(0.01, 10000)
		self.grid_x_spacing.setValue(1.0)
		self.grid_x_spacing.valueChanged.connect(self.update_grid)
		grid_layout.addRow("X Spacing:", self.grid_x_spacing)

		self.grid_y_spacing = QDoubleSpinBox()
		self.grid_y_spacing.setRange(0.01, 10000)
		self.grid_y_spacing.setValue(1.0)
		self.grid_y_spacing.valueChanged.connect(self.update_grid)
		grid_layout.addRow("Y Spacing:", self.grid_y_spacing)

		grid_group.setLayout(grid_layout)
		self.viz_layout.addWidget(grid_group)

		# Reference Lines
		ref_group = QGroupBox("Reference Lines")
		ref_layout = QVBoxLayout()

		ref_btn_layout = QHBoxLayout()
		add_vline_btn = QPushButton("Add Vertical Line")
		add_vline_btn.clicked.connect(
		    lambda: self.add_vertical_reference() if hasattr(self, 'add_vertical_reference') else None)
		add_hline_btn = QPushButton("Add Horizontal Line")
		add_hline_btn.clicked.connect(
		    lambda: self.add_horizontal_reference() if hasattr(self, 'add_horizontal_reference') else None)
		clear_ref_btn = QPushButton("Clear All")
		clear_ref_btn.clicked.connect(
		    lambda: self.clear_reference_lines() if hasattr(self, 'clear_reference_lines') else None)
		ref_btn_layout.addWidget(add_vline_btn)
		ref_btn_layout.addWidget(add_hline_btn)
		ref_btn_layout.addWidget(clear_ref_btn)
		ref_layout.addLayout(ref_btn_layout)

		ref_group.setLayout(ref_layout)
		self.viz_layout.addWidget(ref_group)

		# Region Highlighting
		region_group = QGroupBox("Region Highlighting")
		region_layout = QVBoxLayout()

		region_btn_layout = QHBoxLayout()
		add_region_btn = QPushButton("Add Shaded Region")
		add_region_btn.clicked.connect(
		    lambda: self.add_highlighted_region() if hasattr(self, 'add_highlighted_region') else None)
		clear_region_btn = QPushButton("Clear Regions")
		clear_region_btn.clicked.connect(lambda: self.clear_regions() if hasattr(self, 'clear_regions') else None)
		region_btn_layout.addWidget(add_region_btn)
		region_btn_layout.addWidget(clear_region_btn)
		region_layout.addLayout(region_btn_layout)

		region_group.setLayout(region_layout)
		self.viz_layout.addWidget(region_group)

		# Multiple File Comparison
		multi_group = QGroupBox("Multiple File Comparison")
		multi_layout = QVBoxLayout()

		self.file_list = QListWidget()
		self.file_list.setMaximumHeight(100)
		multi_layout.addWidget(QLabel("Loaded Files:"))
		multi_layout.addWidget(self.file_list)

		multi_btn_layout = QHBoxLayout()
		add_file_btn = QPushButton("Add CSV File")
		add_file_btn.clicked.connect(
		    lambda: self.add_comparison_file() if hasattr(self, 'add_comparison_file') else None)
		remove_file_btn = QPushButton("Remove Selected")
		remove_file_btn.clicked.connect(
		    lambda: self.remove_comparison_file() if hasattr(self, 'remove_comparison_file') else None)
		multi_btn_layout.addWidget(add_file_btn)
		multi_btn_layout.addWidget(remove_file_btn)
		multi_layout.addLayout(multi_btn_layout)

		multi_group.setLayout(multi_layout)
		self.viz_layout.addWidget(multi_group)

		self.viz_layout.addStretch()

	def build_processing_controls(self):
		self.processing_layout.addWidget(QLabel("<b>Data Processing:</b>"))

		# Smoothing
		smooth_group = QGroupBox("Smoothing")
		smooth_layout = QFormLayout()
		self.smooth_enabled = QCheckBox("Enable Smoothing")
		self.smooth_enabled.stateChanged.connect(self.plot_selected)
		smooth_layout.addRow(self.smooth_enabled)

		self.smooth_method = QComboBox()
		if SCIPY_AVAILABLE:
			self.smooth_method.addItems(["Savitzky-Golay", "Gaussian", "Moving Average"])
		else:
			self.smooth_method.addItems(["Moving Average (scipy not installed)"])
		self.smooth_method.currentIndexChanged.connect(self.plot_selected)
		smooth_layout.addRow("Method:", self.smooth_method)

		self.smooth_window = QSpinBox()
		self.smooth_window.setRange(3, 501)
		self.smooth_window.setValue(11)
		self.smooth_window.setSingleStep(1)
		self.smooth_window.valueChanged.connect(self.plot_selected)
		smooth_layout.addRow("Window Size:", self.smooth_window)

		smooth_group.setLayout(smooth_layout)
		self.processing_layout.addWidget(smooth_group)

		# Decimation
		decimate_group = QGroupBox("Data Decimation")
		decimate_layout = QFormLayout()
		self.decimate_enabled = QCheckBox("Enable (for large datasets)")
		self.decimate_enabled.stateChanged.connect(self.plot_selected)
		decimate_layout.addRow(self.decimate_enabled)

		self.decimate_factor = QSpinBox()
		self.decimate_factor.setRange(2, 100)
		self.decimate_factor.setValue(10)
		self.decimate_factor.valueChanged.connect(self.plot_selected)
		decimate_layout.addRow("Factor:", self.decimate_factor)

		decimate_group.setLayout(decimate_layout)
		self.processing_layout.addWidget(decimate_group)

		self.processing_layout.addStretch()

	def build_analysis_controls(self):
		self.analysis_layout.addWidget(QLabel("<b>Data Analysis Tools:</b>"))

		# Curve Fitting
		fit_group = QGroupBox("Curve Fitting")
		fit_layout = QFormLayout()

		self.fit_enabled = QCheckBox("Enable Curve Fit")
		self.fit_enabled.stateChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		fit_layout.addRow(self.fit_enabled)

		self.fit_type = QComboBox()
		self.fit_type.addItems(["Linear", "Polynomial", "Exponential", "Logarithmic", "Power", "Custom Equation"])
		self.fit_type.currentIndexChanged.connect(
		    lambda: self.on_fit_type_changed() if hasattr(self, 'on_fit_type_changed') else None)
		fit_layout.addRow("Fit Type:", self.fit_type)

		# Polynomial degree (shown for polynomial fit)
		self.poly_degree = QSpinBox()
		self.poly_degree.setRange(2, 10)
		self.poly_degree.setValue(2)
		self.poly_degree.valueChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		self.poly_degree_label = QLabel("Poly Degree:")
		fit_layout.addRow(self.poly_degree_label, self.poly_degree)
		self.poly_degree.hide()
		self.poly_degree_label.hide()

		# Custom equation input (shown for custom equation fit)
		self.custom_equation_input = QLineEdit()
		self.custom_equation_input.setPlaceholderText("e.g., a * exp(-b * x)")
		self.custom_equation_input.textChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		self.custom_equation_label = QLabel("Equation:")
		fit_layout.addRow(self.custom_equation_label, self.custom_equation_input)
		self.custom_equation_input.hide()
		self.custom_equation_label.hide()

		# Initial parameters for custom equation
		self.custom_params_input = QLineEdit()
		self.custom_params_input.setPlaceholderText("e.g., 1.0, 0.1")
		self.custom_params_input.textChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		self.custom_params_label = QLabel("Initial Params:")
		fit_layout.addRow(self.custom_params_label, self.custom_params_input)
		self.custom_params_input.hide()
		self.custom_params_label.hide()

		# Help button for custom equations
		self.equation_help_btn = QPushButton("? Help")
		self.equation_help_btn.clicked.connect(self.show_equation_help)
		fit_layout.addRow("", self.equation_help_btn)
		self.equation_help_btn.hide()

		self.fit_series = QComboBox()
		self.fit_series.addItem("(Select series)")
		fit_layout.addRow("Series:", self.fit_series)

		self.show_equation = QCheckBox("Show Equation")
		self.show_equation.setChecked(True)
		self.show_equation.stateChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		fit_layout.addRow(self.show_equation)

		fit_group.setLayout(fit_layout)
		self.analysis_layout.addWidget(fit_group)

		# Peak Detection
		peak_group = QGroupBox("Peak Detection")
		peak_layout = QFormLayout()

		self.peaks_enabled = QCheckBox("Show Peaks")
		self.peaks_enabled.stateChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		peak_layout.addRow(self.peaks_enabled)

		self.peak_prominence = QDoubleSpinBox()
		self.peak_prominence.setRange(0.01, 1000.0)
		self.peak_prominence.setValue(1.0)
		self.peak_prominence.setDecimals(2)
		self.peak_prominence.valueChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		peak_layout.addRow("Prominence:", self.peak_prominence)

		self.peak_series = QComboBox()
		self.peak_series.addItem("(Select series)")
		peak_layout.addRow("Series:", self.peak_series)

		peak_group.setLayout(peak_layout)
		self.analysis_layout.addWidget(peak_group)

		# Derivative
		deriv_group = QGroupBox("Derivative/Rate of Change")
		deriv_layout = QFormLayout()

		self.deriv_enabled = QCheckBox("Show Derivative")
		self.deriv_enabled.stateChanged.connect(
		    lambda: self.update_analysis() if hasattr(self, 'update_analysis') else None)
		deriv_layout.addRow(self.deriv_enabled)

		self.deriv_series = QComboBox()
		self.deriv_series.addItem("(Select series)")
		deriv_layout.addRow("Series:", self.deriv_series)

		deriv_group.setLayout(deriv_layout)
		self.analysis_layout.addWidget(deriv_group)

		# FFT Analysis
		fft_group = QGroupBox("FFT/Frequency Analysis")
		fft_layout = QFormLayout()

		self.fft_enabled = QCheckBox("Show FFT")
		self.fft_enabled.stateChanged.connect(
		    lambda: self.show_fft_window() if hasattr(self, 'show_fft_window') else None)
		fft_layout.addRow(self.fft_enabled)

		self.fft_series = QComboBox()
		self.fft_series.addItem("(Select series)")
		fft_layout.addRow("Series:", self.fft_series)

		fft_group.setLayout(fft_layout)
		self.analysis_layout.addWidget(fft_group)

		# Statistics Display
		self.analysis_results = QTextEdit()
		self.analysis_results.setReadOnly(True)
		self.analysis_results.setMaximumHeight(150)
		self.analysis_results.setPlaceholderText("Analysis results will appear here...")
		self.analysis_layout.addWidget(QLabel("<b>Analysis Results:</b>"))
		self.analysis_layout.addWidget(self.analysis_results)

		self.analysis_layout.addStretch()

		# Update series dropdowns when selections change
		self.update_analysis_series_lists()

	def build_help_tab(self):
		"""Build the help/documentation tab"""
		self.help_layout.addWidget(QLabel("<b>User Manual & Documentation</b>"))

		# Search bar for help content
		search_layout = QHBoxLayout()
		search_layout.addWidget(QLabel("Search:"))
		self.help_search = QLineEdit()
		self.help_search.setPlaceholderText("Search documentation...")
		self.help_search.textChanged.connect(self.search_help)
		search_layout.addWidget(self.help_search)
		self.help_layout.addLayout(search_layout)

		# Use QTextBrowser for rendered HTML display
		self.help_browser = QTextBrowser()
		self.help_browser.setReadOnly(True)
		self.help_browser.setOpenExternalLinks(False)  # Handle links internally
		self.help_browser.anchorClicked.connect(self.handle_help_link)
		self.help_layout.addWidget(self.help_browser)

		# Status label for markdown library
		if not MARKDOWN_AVAILABLE:
			status_label = QLabel("📝 Install 'markdown' package for better formatting: pip install markdown")
			status_label.setWordWrap(True)
			status_label.setStyleSheet("color: orange; font-style: italic;")
			self.help_layout.addWidget(status_label)

		# Buttons
		btn_layout = QHBoxLayout()
		reload_btn = QPushButton("Reload Help File")
		reload_btn.clicked.connect(self.load_help_file)
		export_btn = QPushButton("Export as HTML")
		export_btn.clicked.connect(self.export_help_html)
		btn_layout.addWidget(reload_btn)
		btn_layout.addWidget(export_btn)
		btn_layout.addStretch()
		self.help_layout.addLayout(btn_layout)

		# Load the help file
		self.load_help_file()

	def load_help_file(self):
		"""Load and display the markdown help file"""
		help_file = Path(__file__).parent / "plot_viewer_help.md"

		if help_file.exists():
			try:
				with open(help_file, 'r', encoding='utf-8') as f:
					markdown_content = f.read()

				self.help_full_content = markdown_content

				# Render markdown to HTML
				if MARKDOWN_AVAILABLE:
					html_content = markdown.markdown(markdown_content, extensions=['tables', 'fenced_code', 'toc'])
					styled_html = self.style_help_html(html_content)
					self.help_browser.setHtml(styled_html)
				else:
					# Fallback: basic formatting
					styled_html = self.style_help_html(f"<pre>{markdown_content}</pre>")
					self.help_browser.setHtml(styled_html)

			except Exception as e:
				self.help_browser.setPlainText(
				    f"Error loading help file: {e}\n\nPlace 'plot_viewer_help.md' in the same directory as this script."
				)
		else:
			# Show instructions if file doesn't exist
			self.help_browser.setHtml(
			    self.style_help_html(
			        f"""
                <h2>Help File Not Found</h2>
                <p>Create a file named <code>plot_viewer_help.md</code> in the same directory as the plot viewer script.</p>
                <p>Expected location: <code>{help_file}</code></p>
                <p>The file should contain the user manual in Markdown format.</p>
            """))

	def style_help_html(self, html_content):
		"""Add theme-aware CSS styling to HTML content"""
		# Theme-aware colors
		if self.theme_dark:
			bg_color = '#1e1e1e'
			text_color = '#d4d4d4'
			heading_color = '#569cd6'
			subheading_color = '#4ec9b0'
			code_bg = '#2d2d2d'
			code_text = '#ce9178'
			link_color = '#569cd6'
			border_color = '#404040'
			table_border = '#404040'
		else:
			bg_color = '#ffffff'
			text_color = '#24292e'
			heading_color = '#0366d6'
			subheading_color = '#0366d6'
			code_bg = '#f6f8fa'
			code_text = '#e36209'
			link_color = '#0366d6'
			border_color = '#e1e4e8'
			table_border = '#d0d7de'

		# GitHub-like styling
		styled_html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <style>
                body {{
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif;
                    line-height: 1.6;
                    color: {text_color};
                    background-color: {bg_color};
                    padding: 20px;
                    max-width: 980px;
                }}
                h1 {{
                    border-bottom: 1px solid {border_color};
                    padding-bottom: 0.3em;
                    font-size: 2em;
                    margin-top: 24px;
                    margin-bottom: 16px;
                    font-weight: 600;
                    color: {heading_color};
                }}
                h2 {{
                    border-bottom: 1px solid {border_color};
                    padding-bottom: 0.3em;
                    font-size: 1.5em;
                    margin-top: 24px;
                    margin-bottom: 16px;
                    font-weight: 600;
                    color: {subheading_color};
                }}
                h3 {{
                    font-size: 1.25em;
                    margin-top: 24px;
                    margin-bottom: 16px;
                    font-weight: 600;
                    color: {subheading_color};
                }}
                h4, h5, h6 {{
                    margin-top: 24px;
                    margin-bottom: 16px;
                    font-weight: 600;
                    color: {text_color};
                }}
                p {{
                    margin-top: 0;
                    margin-bottom: 10px;
                }}
                code {{
                    background-color: {code_bg};
                    color: {code_text};
                    padding: 0.2em 0.4em;
                    margin: 0;
                    font-size: 85%;
                    border-radius: 3px;
                    font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, 'Courier New', monospace;
                }}
                pre {{
                    background-color: {code_bg};
                    border-radius: 6px;
                    padding: 16px;
                    overflow: auto;
                    font-size: 85%;
                    line-height: 1.45;
                    margin-bottom: 16px;
                }}
                pre code {{
                    background-color: transparent;
                    border: 0;
                    display: inline;
                    padding: 0;
                    margin: 0;
                    overflow: visible;
                    line-height: inherit;
                }}
                a {{
                    color: {link_color};
                    text-decoration: none;
                }}
                a:hover {{
                    text-decoration: underline;
                }}
                ul, ol {{
                    padding-left: 2em;
                    margin-top: 0;
                    margin-bottom: 16px;
                }}
                li {{
                    margin-top: 0.25em;
                }}
                blockquote {{
                    margin: 0;
                    padding: 0 1em;
                    color: {text_color};
                    border-left: 0.25em solid {border_color};
                }}
                table {{
                    border-spacing: 0;
                    border-collapse: collapse;
                    margin-top: 0;
                    margin-bottom: 16px;
                }}
                table th {{
                    font-weight: 600;
                    padding: 6px 13px;
                    border: 1px solid {table_border};
                }}
                table td {{
                    padding: 6px 13px;
                    border: 1px solid {table_border};
                }}
                hr {{
                    height: 0.25em;
                    padding: 0;
                    margin: 24px 0;
                    background-color: {border_color};
                    border: 0;
                }}
                strong {{
                    font-weight: 600;
                }}
            </style>
        </head>
        <body>
            {html_content}
        </body>
        </html>
        """
		return styled_html

	def handle_help_link(self, url):
		"""Handle internal anchor links in help documentation"""
		# Check if it's an internal anchor link
		url_str = url.toString()
		if url_str.startswith('#'):
			# Scroll to anchor
			self.help_browser.scrollToAnchor(url_str[1:])
		else:
			# External link - open in browser
			from PyQt5.QtGui import QDesktopServices
			QDesktopServices.openUrl(url)

	def search_help(self, text):
		"""Search within help documentation"""
		if not text:
			self.load_help_file()
			return

		# Use Qt's built-in find function
		self.help_browser.find(text)

	def export_help_html(self):
		"""Export help documentation as HTML file"""
		filename, _ = QFileDialog.getSaveFileName(
		    self, "Export Help as HTML", "plot_viewer_help.html", "HTML Files (*.html)")

		if filename:
			try:
				if MARKDOWN_AVAILABLE and hasattr(self, 'help_full_content'):
					html_content = markdown.markdown(
					    self.help_full_content, extensions=['tables', 'fenced_code', 'toc'])
					styled_html = self.style_help_html(html_content)
				else:
					styled_html = self.help_browser.toHtml()

				with open(filename, 'w', encoding='utf-8') as f:
					f.write(styled_html)
				self.statusBar().showMessage(f"Help exported to {filename}")
			except Exception as e:
				QMessageBox.critical(self, "Export Error", str(e))

	def update_analysis_series_lists(self):
		"""Update the series selection dropdowns in analysis tab"""
		current_fit = self.fit_series.currentText() if hasattr(self, 'fit_series') else ""
		current_peak = self.peak_series.currentText() if hasattr(self, 'peak_series') else ""
		current_deriv = self.deriv_series.currentText() if hasattr(self, 'deriv_series') else ""
		current_fft = self.fft_series.currentText() if hasattr(self, 'fft_series') else ""

		if hasattr(self, 'fit_series'):
			self.fit_series.clear()
			self.fit_series.addItem("(Select series)")
			self.peak_series.clear()
			self.peak_series.addItem("(Select series)")
			self.deriv_series.clear()
			self.deriv_series.addItem("(Select series)")
			self.fft_series.clear()
			self.fft_series.addItem("(Select series)")

			for item in self.y1_list.selectedItems():
				self.fit_series.addItem(item.text())
				self.peak_series.addItem(item.text())
				self.deriv_series.addItem(item.text())
				self.fft_series.addItem(item.text())

			for item in self.y2_list.selectedItems():
				self.fit_series.addItem(item.text())
				self.peak_series.addItem(item.text())
				self.deriv_series.addItem(item.text())
				self.fft_series.addItem(item.text())

			# Restore previous selections if still available
			idx = self.fit_series.findText(current_fit)
			if idx >= 0:
				self.fit_series.setCurrentIndex(idx)
			idx = self.peak_series.findText(current_peak)
			if idx >= 0:
				self.peak_series.setCurrentIndex(idx)
			idx = self.deriv_series.findText(current_deriv)
			if idx >= 0:
				self.deriv_series.setCurrentIndex(idx)
			idx = self.fft_series.findText(current_fft)
			if idx >= 0:
				self.fft_series.setCurrentIndex(idx)

	def create_menu_bar(self):
		menubar = self.menuBar()

		# File Menu
		file_menu = menubar.addMenu("&File")

		open_action = QAction("&Open CSV...", self)
		open_action.setShortcut(QKeySequence.Open)
		open_action.triggered.connect(self.open_file_dialog)
		file_menu.addAction(open_action)

		# Recent Files
		self.recent_menu = file_menu.addMenu("Recent Files")
		self.update_recent_menu()

		file_menu.addSeparator()

		save_plot_action = QAction("&Save Plot...", self)
		save_plot_action.setShortcut(QKeySequence.Save)
		save_plot_action.triggered.connect(self.save_plot_view)
		file_menu.addAction(save_plot_action)

		export_data_action = QAction("Export Filtered &Data...", self)
		export_data_action.triggered.connect(self.export_data)
		file_menu.addAction(export_data_action)

		file_menu.addSeparator()

		add_comparison_action = QAction("Add &Comparison CSV...", self)
		add_comparison_action.setShortcut("Ctrl+M")
		add_comparison_action.triggered.connect(
		    lambda: self.add_comparison_file() if hasattr(self, 'add_comparison_file') else None)
		file_menu.addAction(add_comparison_action)

		file_menu.addSeparator()

		exit_action = QAction("E&xit", self)
		exit_action.setShortcut(QKeySequence.Quit)
		exit_action.triggered.connect(self.close)
		file_menu.addAction(exit_action)

		# View Menu
		view_menu = menubar.addMenu("&View")

		self.dark_theme_action = QAction("&Dark Theme", self, checkable=True)
		self.dark_theme_action.setChecked(self.theme_dark)
		self.dark_theme_action.triggered.connect(self.toggle_theme)
		view_menu.addAction(self.dark_theme_action)

		self.crosshair_action = QAction("&Crosshair", self, checkable=True)
		self.crosshair_action.setShortcut("C")
		self.crosshair_action.triggered.connect(self.toggle_crosshair)
		view_menu.addAction(self.crosshair_action)

		view_menu.addSeparator()

		# Toggleable dock widgets
		self.toggle_summary_action = QAction("Column &Summary", self, checkable=True)
		self.toggle_summary_action.setChecked(True)
		self.toggle_summary_action.triggered.connect(
		    lambda: self.csv_dock.setVisible(self.toggle_summary_action.isChecked()))
		view_menu.addAction(self.toggle_summary_action)

		self.toggle_stats_action = QAction("Dataset S&tatistics", self, checkable=True)
		self.toggle_stats_action.setChecked(True)
		self.toggle_stats_action.triggered.connect(
		    lambda: self.stats_dock.setVisible(self.toggle_stats_action.isChecked()))
		view_menu.addAction(self.toggle_stats_action)

		view_menu.addSeparator()

		self.log_x_action = QAction("Logarithmic &X-Axis", self, checkable=True)
		self.log_x_action.triggered.connect(self.toggle_log_x)
		view_menu.addAction(self.log_x_action)

		self.log_y_action = QAction("Logarithmic &Y-Axis (Left)", self, checkable=True)
		self.log_y_action.triggered.connect(self.toggle_log_y)
		view_menu.addAction(self.log_y_action)

		# Plot Menu
		plot_menu = menubar.addMenu("&Plot")

		plot_action = QAction("&Update Plot", self)
		plot_action.setShortcut("Ctrl+P")
		plot_action.triggered.connect(self.plot_selected)
		plot_menu.addAction(plot_action)

		clear_selections_action = QAction("Clear &Selections", self)
		clear_selections_action.setShortcut("Ctrl+D")
		clear_selections_action.triggered.connect(self.clear_all_selections)
		plot_menu.addAction(clear_selections_action)

		reset_zoom_action = QAction("&Reset Zoom", self)
		reset_zoom_action.setShortcut("R")
		reset_zoom_action.triggered.connect(self.reset_zoom)
		plot_menu.addAction(reset_zoom_action)

		plot_menu.addSeparator()

		add_annotation_action = QAction("Add &Annotation...", self)
		add_annotation_action.setShortcut("Ctrl+A")
		add_annotation_action.triggered.connect(self.add_annotation)
		plot_menu.addAction(add_annotation_action)

		clear_annotations_action = QAction("Clear Annotations", self)
		clear_annotations_action.triggered.connect(self.clear_annotations)
		plot_menu.addAction(clear_annotations_action)

		plot_menu.addSeparator()

		set_title_action = QAction("Set Plot &Title...", self)
		set_title_action.setShortcut("Ctrl+T")
		set_title_action.triggered.connect(self.set_plot_title)
		plot_menu.addAction(set_title_action)

		set_labels_action = QAction("Set Axis &Labels...", self)
		set_labels_action.setShortcut("Ctrl+L")
		set_labels_action.triggered.connect(self.set_axis_labels)
		plot_menu.addAction(set_labels_action)

		plot_menu.addSeparator()

		add_vline_action = QAction("Add &Vertical Reference Line...", self)
		add_vline_action.triggered.connect(
		    lambda: self.add_vertical_reference() if hasattr(self, 'add_vertical_reference') else None)
		plot_menu.addAction(add_vline_action)

		add_hline_action = QAction("Add &Horizontal Reference Line...", self)
		add_hline_action.triggered.connect(
		    lambda: self.add_horizontal_reference() if hasattr(self, 'add_horizontal_reference') else None)
		plot_menu.addAction(add_hline_action)

		add_region_action = QAction("Add Highlighted &Region...", self)
		add_region_action.triggered.connect(
		    lambda: self.add_highlighted_region() if hasattr(self, 'add_highlighted_region') else None)
		plot_menu.addAction(add_region_action)

		# Analysis Menu
		analysis_menu = menubar.addMenu("&Analysis")

		curve_fit_action = QAction("Curve &Fitting...", self)
		curve_fit_action.triggered.connect(lambda: self.tabs.setCurrentIndex(3))  # Switch to Analysis tab
		analysis_menu.addAction(curve_fit_action)

		peak_detect_action = QAction("&Peak Detection...", self)
		peak_detect_action.triggered.connect(lambda: self.tabs.setCurrentIndex(3))
		analysis_menu.addAction(peak_detect_action)

		derivative_action = QAction("&Derivative...", self)
		derivative_action.triggered.connect(lambda: self.tabs.setCurrentIndex(3))
		analysis_menu.addAction(derivative_action)

		fft_action = QAction("&FFT Analysis...", self)
		fft_action.triggered.connect(lambda: self.tabs.setCurrentIndex(3))
		analysis_menu.addAction(fft_action)

		# Help Menu
		help_menu = menubar.addMenu("&Help")

		shortcuts_action = QAction("&Keyboard Shortcuts", self)
		shortcuts_action.setShortcut("F1")
		shortcuts_action.triggered.connect(self.show_shortcuts)
		help_menu.addAction(shortcuts_action)

		about_action = QAction("&About", self)
		about_action.triggered.connect(self.show_about)
		help_menu.addAction(about_action)

	def create_toolbar(self):
		toolbar = QToolBar("Main Toolbar")
		toolbar.setMovable(False)
		self.addToolBar(toolbar)

		open_btn = QAction("📂 Open", self)
		open_btn.triggered.connect(self.open_file_dialog)
		toolbar.addAction(open_btn)

		toolbar.addSeparator()

		self.zoom_action = QAction("🔍 Box Zoom", self, checkable=True)
		self.zoom_action.triggered.connect(self.toggle_zoom_mode)
		toolbar.addAction(self.zoom_action)

		reset_btn = QAction("↻ Reset Zoom", self)
		reset_btn.triggered.connect(self.reset_zoom)
		toolbar.addAction(reset_btn)

		toolbar.addSeparator()

		clear_btn = QAction("🗑️ Clear Selections", self)
		clear_btn.triggered.connect(self.clear_all_selections)
		toolbar.addAction(clear_btn)

		plot_btn = QAction("📊 Plot", self)
		plot_btn.triggered.connect(self.plot_selected)
		toolbar.addAction(plot_btn)

		save_btn = QAction("💾 Save", self)
		save_btn.triggered.connect(self.save_plot_view)
		toolbar.addAction(save_btn)

	def update_recent_menu(self):
		self.recent_menu.clear()
		for file_path in self.recent_files:
			if os.path.exists(file_path):
				action = QAction(os.path.basename(file_path), self)
				action.setData(file_path)
				action.triggered.connect(lambda checked, f=file_path: self.load_csv(f))
				self.recent_menu.addAction(action)

		if self.recent_files:
			self.recent_menu.addSeparator()
			clear_action = QAction("Clear Recent", self)
			clear_action.triggered.connect(self.clear_recent)
			self.recent_menu.addAction(clear_action)

	def add_to_recent(self, file_path):
		if file_path in self.recent_files:
			self.recent_files.remove(file_path)
		self.recent_files.insert(0, file_path)
		self.recent_files = self.recent_files[:10]
		self.settings.setValue("recent_files", self.recent_files)
		self.update_recent_menu()

	def clear_recent(self):
		self.recent_files = []
		self.settings.setValue("recent_files", [])
		self.update_recent_menu()

	def select_all_items(self, list_widget):
		for i in range(list_widget.count()):
			list_widget.item(i).setSelected(True)

	def select_all_columns(self):
		"""Select all items in both left and right Y-axis lists"""
		for i in range(self.y1_list.count()):
			self.y1_list.item(i).setSelected(True)
		for i in range(self.y2_list.count()):
			self.y2_list.item(i).setSelected(True)

	def clear_all_selections(self):
		"""Clear all selections from both Y-axis lists"""
		self.y1_list.clearSelection()
		self.y2_list.clearSelection()

	def filter_columns(self, text):
		"""Filter column lists based on search text"""
		text = text.lower()
		for list_widget in [self.y1_list, self.y2_list]:
			for i in range(list_widget.count()):
				item = list_widget.item(i)
				item.setHidden(text and text not in item.text().lower())

	def update_recent_columns(self):
		"""Track recently used columns"""
		columns = []
		if self.x_selector.currentText():
			columns.append(self.x_selector.currentText())
		for item in self.y1_list.selectedItems():
			columns.append(item.text())
		for item in self.y2_list.selectedItems():
			columns.append(item.text())

		for col in columns:
			if col in self.recent_columns:
				self.recent_columns.remove(col)
			self.recent_columns.insert(0, col)

		self.recent_columns = self.recent_columns[:20]
		self.settings.setValue("recent_columns", self.recent_columns)

		# Update recent columns display if it exists
		if hasattr(self, 'recent_label'):
			self.recent_label.setText(", ".join(self.recent_columns[:5]))

	def apply_theme(self):
		if self.theme_dark:
			pg.setConfigOption('background', '#1e1e1e')
			pg.setConfigOption('foreground', '#ffffff')

			# Dark theme stylesheet
			self.setStyleSheet(
			    """
                QWidget {
                    background-color: #2b2b2b;
                    color: #ffffff;
                }
                QComboBox {
                    background-color: #3c3c3c;
                    color: #ffffff;
                    border: 1px solid #555555;
                    padding: 3px;
                }
                QComboBox:hover {
                    border: 1px solid #0078d4;
                }
                QComboBox::drop-down {
                    border: none;
                }
                QListWidget {
                    background-color: #3c3c3c;
                    color: #ffffff;
                    border: 1px solid #555555;
                }
                QListWidget::item:selected {
                    background-color: #0078d4;
                }
                QTableWidget {
                    background-color: #3c3c3c;
                    color: #ffffff;
                    gridline-color: #555555;
                }
                QHeaderView::section {
                    background-color: #2b2b2b;
                    color: #ffffff;
                    border: 1px solid #555555;
                }
                QTextEdit {
                    background-color: #3c3c3c;
                    color: #ffffff;
                    border: 1px solid #555555;
                }
                QPushButton {
                    background-color: #3c3c3c;
                    color: #ffffff;
                    border: 1px solid #555555;
                    padding: 5px;
                }
                QPushButton:hover {
                    background-color: #505050;
                    border: 1px solid #0078d4;
                }
                QGroupBox {
                    color: #ffffff;
                    border: 1px solid #555555;
                    margin-top: 6px;
                }
                QGroupBox::title {
                    color: #ffffff;
                }
                QLabel {
                    color: #ffffff;
                }
                QCheckBox {
                    color: #ffffff;
                }
                QSpinBox, QDoubleSpinBox {
                    background-color: #3c3c3c;
                    color: #ffffff;
                    border: 1px solid #555555;
                }
            """)
		else:
			pg.setConfigOption('background', '#ffffff')
			pg.setConfigOption('foreground', '#000000')

			# Light theme stylesheet
			self.setStyleSheet(
			    """
                QWidget {
                    background-color: #f0f0f0;
                    color: #000000;
                }
                QComboBox {
                    background-color: #ffffff;
                    color: #000000;
                    border: 1px solid #c0c0c0;
                    padding: 3px;
                }
                QComboBox:hover {
                    border: 1px solid #0078d4;
                }
                QListWidget {
                    background-color: #ffffff;
                    color: #000000;
                    border: 1px solid #c0c0c0;
                }
                QListWidget::item:selected {
                    background-color: #0078d4;
                    color: #ffffff;
                }
                QTableWidget {
                    background-color: #ffffff;
                    color: #000000;
                    gridline-color: #c0c0c0;
                }
                QHeaderView::section {
                    background-color: #e0e0e0;
                    color: #000000;
                    border: 1px solid #c0c0c0;
                }
                QTextEdit {
                    background-color: #ffffff;
                    color: #000000;
                    border: 1px solid #c0c0c0;
                }
                QPushButton {
                    background-color: #ffffff;
                    color: #000000;
                    border: 1px solid #c0c0c0;
                    padding: 5px;
                }
                QPushButton:hover {
                    background-color: #e0e0e0;
                    border: 1px solid #0078d4;
                }
                QGroupBox {
                    color: #000000;
                    border: 1px solid #c0c0c0;
                    margin-top: 6px;
                }
                QLabel {
                    color: #000000;
                }
                QCheckBox {
                    color: #000000;
                }
                QSpinBox, QDoubleSpinBox {
                    background-color: #ffffff;
                    color: #000000;
                    border: 1px solid #c0c0c0;
                }
            """)

	def update_style_selectors(self):
		while self.style_form.count():
			item = self.style_form.takeAt(0)
			if item.widget():
				item.widget().deleteLater()

		self.series_style = {}

		# Color options based on theme - exclude invisible colors
		if self.theme_dark:
			colors = [
			    "White", "Red", "Green", "Blue", "Magenta", "Cyan", "Yellow", "Orange", "Purple", "Brown", "Pink",
			    "Lime", "Navy", "Teal", "Maroon", "Olive"
			]
		else:
			colors = [
			    "Black", "Red", "Green", "Blue", "Magenta", "Cyan", "Yellow", "Orange", "Purple", "Brown", "Pink",
			    "Lime", "Navy", "Teal", "Maroon", "Olive"
			]

		def add_style_rows(label_prefix, items):
			for item in items:
				name = item.text()

				# Visibility toggle
				visible_check = QCheckBox("Visible")
				visible_check.setChecked(self.series_visibility.get(name, True))
				visible_check.stateChanged.connect(self.plot_selected)

				line_style = QComboBox()
				line_style.addItems(["Solid", "None", "Dashed", "Dotted"])

				marker_style = QComboBox()
				marker_style.addItems(["None", "o", "s", "t", "d", "+", "x"])

				color_style = QComboBox()
				color_style.addItems(colors)

				line_width = QDoubleSpinBox()
				line_width.setRange(0.5, 10.0)
				line_width.setValue(2.0)
				line_width.setSingleStep(0.5)

				alpha_slider = QSlider(Qt.Horizontal)
				alpha_slider.setRange(10, 100)
				alpha_slider.setValue(100)
				alpha_label = QLabel("100%")
				alpha_slider.valueChanged.connect(lambda v, lbl=alpha_label: lbl.setText(f"{v}%"))

				# Load saved styles or use defaults
				if name in self.series_saved_styles:
					style = self.series_saved_styles[name]
					saved_line = style.get("line", "Solid")
					saved_marker = style.get("marker", "None")
					# Restore saved styles
					line_style.setCurrentText(
					    saved_line if saved_line in ["Solid", "None", "Dashed", "Dotted"] else "Solid")
					marker_style.setCurrentText(saved_marker)
					color_style.setCurrentText(style.get("color", "Black" if not self.theme_dark else "White"))
					line_width.setValue(style.get("width", 2.0))
					alpha_slider.setValue(style.get("alpha", 100))
				else:
					# Set defaults for new series: Solid line, No marker
					line_style.setCurrentText("Solid")
					marker_style.setCurrentText("None")

				# Auto-set line to None when marker is changed from None to something else
				def on_marker_changed(text, line_combo=line_style):
					if text != "None":
						line_combo.setCurrentText("None")

				marker_style.currentTextChanged.connect(on_marker_changed)

				self.series_style[name] = {
				    "visible": visible_check,
				    "line": line_style,
				    "marker": marker_style,
				    "color": color_style,
				    "width": line_width,
				    "alpha": alpha_slider
				}
				self.series_visibility[name] = visible_check.isChecked()

				group = QGroupBox(f"{label_prefix}: {name}")
				group_layout = QFormLayout(group)
				group_layout.addRow("Visible:", visible_check)
				group_layout.addRow("Line:", line_style)
				group_layout.addRow("Marker:", marker_style)
				group_layout.addRow("Color:", color_style)
				group_layout.addRow("Width:", line_width)
				alpha_layout = QHBoxLayout()
				alpha_layout.addWidget(alpha_slider)
				alpha_layout.addWidget(alpha_label)
				group_layout.addRow("Opacity:", alpha_layout)

				self.style_form.addRow(group)

		add_style_rows("Left", self.y1_list.selectedItems())
		add_style_rows("Right", self.y2_list.selectedItems())

		# Update analysis series lists
		self.update_analysis_series_lists()

	def restore_selections(self):
		saved_x = self.settings.value("x_column", "", type=str)
		saved_y1 = self.settings.value("y1_columns", [], type=list)
		saved_y2 = self.settings.value("y2_columns", [], type=list)

		if saved_x:
			idx = self.x_selector.findText(saved_x)
			if idx >= 0:
				self.x_selector.setCurrentIndex(idx)

		def select_items(list_widget, names):
			for i in range(list_widget.count()):
				item = list_widget.item(i)
				if item.text() in names:
					item.setSelected(True)

		select_items(self.y1_list, saved_y1)
		select_items(self.y2_list, saved_y2)
		self.update_style_selectors()

	def replace_plot_widget(self):
		self.apply_theme()
		zoom_x = zoom_y = None
		if hasattr(self, 'main_plot'):
			zoom_x, zoom_y = self.main_plot.getViewBox().viewRange()

		if hasattr(self, 'plot_area'):
			self.plot_area.setParent(None)

		self.plot_area = pg.GraphicsLayoutWidget()
		self.main_plot = self.plot_area.addPlot()
		self.main_plot.showGrid(x=True, y=True, alpha=0.3)
		self.main_plot.setLabel('bottom', '')
		self.main_plot.setLabel('left', '')

		# Add legend - Note: pyqtgraph legends are not directly draggable
		# but they anchor to the plot corner
		self.legend = self.main_plot.addLegend(offset=(10, 10))

		# Add plot title if set
		if self.plot_title:
			self.main_plot.setTitle(self.plot_title, size='12pt')

		self.right_view = pg.ViewBox()
		self.main_plot.showAxis('right')
		self.main_plot.scene().addItem(self.right_view)
		self.main_plot.getAxis('right').linkToView(self.right_view)
		self.right_view.setXLink(self.main_plot)
		self.main_plot.getViewBox().sigResized.connect(
		    lambda: self.right_view.setGeometry(self.main_plot.getViewBox().sceneBoundingRect()))

		# Crosshair setup
		self.vLine = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen('y', width=1, style=Qt.DashLine))
		self.hLine = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('y', width=1, style=Qt.DashLine))
		self.main_plot.addItem(self.vLine, ignoreBounds=True)
		self.main_plot.addItem(self.hLine, ignoreBounds=True)
		self.vLine.setVisible(False)
		self.hLine.setVisible(False)

		# Restore reference lines
		for line in self.reference_lines:
			self.main_plot.addItem(line)

		# Restore highlighted regions
		for region in self.highlighted_regions:
			self.main_plot.addItem(region)

		# Restore tooltip if enabled
		if hasattr(self, 'tooltip_enabled') and self.tooltip_enabled.isChecked():
			if self.data_tooltip is not None:
				self.main_plot.addItem(self.data_tooltip)

		self.proxy = pg.SignalProxy(self.main_plot.scene().sigMouseMoved, rateLimit=60, slot=self.mouse_moved)

		self.splitter.insertWidget(0, self.plot_area)

		if self.df is not None:
			self.load_column_selectors()
			self.restore_selections()
			self.plot_selected()
			if zoom_x and zoom_y:
				self.main_plot.setXRange(*zoom_x, padding=0)
				self.main_plot.setYRange(*zoom_y, padding=0)

	def toggle_theme(self):
		self.theme_dark = self.dark_theme_action.isChecked()
		self.theme_checkbox.setChecked(self.theme_dark)
		self.settings.setValue("theme_dark", self.theme_dark)
		self.replace_plot_widget()
		# Reload help content with new theme
		if hasattr(self, 'help_browser'):
			self.load_help_file()

	def toggle_theme_checkbox(self):
		self.theme_dark = self.theme_checkbox.isChecked()
		self.dark_theme_action.setChecked(self.theme_dark)
		self.settings.setValue("theme_dark", self.theme_dark)
		self.replace_plot_widget()
		# Reload help content with new theme
		if hasattr(self, 'help_browser'):
			self.load_help_file()

	def toggle_crosshair(self):
		self.crosshair_enabled = self.crosshair_action.isChecked()
		self.vLine.setVisible(self.crosshair_enabled)
		self.hLine.setVisible(self.crosshair_enabled)

	def mouse_moved(self, evt):
		if self.df is None:
			return

		pos = evt[0]
		if self.main_plot.sceneBoundingRect().contains(pos):
			mousePoint = self.main_plot.vb.mapSceneToView(pos)

			# Update crosshair
			if self.crosshair_enabled:
				self.vLine.setPos(mousePoint.x())
				self.hLine.setPos(mousePoint.y())

			# Update tooltip
			if self.tooltip_enabled.isChecked():
				result = self.find_nearest_point(mousePoint.x())
				if result:
					x_val, info = result

					# Format tooltip text
					tooltip_text = ""
					for key, val in info.items():
						tooltip_text += f"{key}: {val:.4g}\n"

					# Update tooltip
					if self.data_tooltip is None:
						self.data_tooltip = pg.TextItem(anchor=(0, 1), color='y')
						self.main_plot.addItem(self.data_tooltip)

					self.data_tooltip.setText(tooltip_text.strip())
					self.data_tooltip.setPos(x_val, mousePoint.y())
			else:
				if self.data_tooltip is not None:
					self.main_plot.removeItem(self.data_tooltip)
					self.data_tooltip = None

			# Status bar update
			self.statusBar().showMessage(f"x={mousePoint.x():.4f}, y={mousePoint.y():.4f}")

	def toggle_log_x(self):
		self.main_plot.setLogMode(x=self.log_x_action.isChecked())

	def toggle_log_y(self):
		self.main_plot.setLogMode(y=self.log_y_action.isChecked())

	def dragEnterEvent(self, event: QDragEnterEvent):
		if event.mimeData().hasUrls():
			event.acceptProposedAction()

	def dropEvent(self, event: QDropEvent):
		files = [u.toLocalFile() for u in event.mimeData().urls()]
		for f in files:
			if f.lower().endswith('.csv'):
				self.load_csv(f)
				break

	def open_file_dialog(self):
		start_dir = self.settings.value("last_csv_dir", os.path.expanduser("~"))
		filename, _ = QFileDialog.getOpenFileName(self, "Open CSV File", start_dir, "CSV Files (*.csv);;All Files (*)")
		if filename:
			self.load_csv(filename)

	def load_csv(self, filename):
		try:
			self.df = pd.read_csv(filename, on_bad_lines='warn')
		except Exception as e:
			QMessageBox.critical(self, "Error", f"Failed to read CSV: {e}")
			return

		self.statusBar().showMessage(f"Loaded: {os.path.basename(filename)} ({len(self.df)} rows)")
		self.csv_path = filename
		self.csv_mtime = os.path.getmtime(filename)
		self.settings.setValue("last_csv_file", filename)
		self.settings.setValue("last_csv_dir", os.path.dirname(filename))
		self.add_to_recent(filename)
		self.load_column_selectors()
		self.restore_selections()
		self.plot_selected()
		self.update_csv_preview()
		self.update_statistics()

	def load_column_selectors(self):
		self.x_selector.clear()
		self.y1_list.clear()
		self.y2_list.clear()

		all_columns = set()

		# Add columns from main dataframe
		if self.df is not None:
			for col in self.df.columns:
				if pd.api.types.is_numeric_dtype(self.df[col]):
					all_columns.add(col)

		# Add columns from comparison files
		for file_name, df in self.loaded_files.items():
			for col in df.columns:
				if pd.api.types.is_numeric_dtype(df[col]):
					all_columns.add(f"{col} ({file_name})")

		# Populate selectors
		for col in sorted(all_columns):
			self.x_selector.addItem(col)
			self.y1_list.addItem(QListWidgetItem(col))
			self.y2_list.addItem(QListWidgetItem(col))

	def get_pen(self, style_name, color_name, width=2.0, alpha=100):
		# Return None if line style is "None" - this prevents line drawing
		if style_name == "None":
			return None

		pen_styles = {"Solid": Qt.SolidLine, "Dashed": Qt.DashLine, "Dotted": Qt.DotLine}
		color_map = {
		    "Black": (0, 0, 0),
		    "White": (255, 255, 255),
		    "Red": (255, 0, 0),
		    "Green": (0, 255, 0),
		    "Blue": (0, 0, 255),
		    "Magenta": (255, 0, 255),
		    "Cyan": (0, 255, 255),
		    "Yellow": (255, 255, 0),
		    "Gray": (128, 128, 128),
		    "Orange": (255, 165, 0),
		    "Purple": (128, 0, 128),
		    "Brown": (165, 42, 42),
		    "Pink": (255, 192, 203),
		    "Lime": (0, 255, 0),
		    "Navy": (0, 0, 128),
		    "Teal": (0, 128, 128),
		    "Maroon": (128, 0, 0),
		    "Olive": (128, 128, 0)
		}

		# Get RGB tuple and add alpha
		rgb = color_map.get(color_name, (0, 0, 0))
		color = (*rgb, int(255 * alpha / 100))

		return pg.mkPen(color=color, width=width, style=pen_styles.get(style_name, Qt.SolidLine))

	def get_brush(self, color_name, alpha=100):
		"""Get brush with alpha for markers"""
		color_map = {
		    "Black": (0, 0, 0),
		    "White": (255, 255, 255),
		    "Red": (255, 0, 0),
		    "Green": (0, 255, 0),
		    "Blue": (0, 0, 255),
		    "Magenta": (255, 0, 255),
		    "Cyan": (0, 255, 255),
		    "Yellow": (255, 255, 0),
		    "Gray": (128, 128, 128),
		    "Orange": (255, 165, 0),
		    "Purple": (128, 0, 128),
		    "Brown": (165, 42, 42),
		    "Pink": (255, 192, 203),
		    "Lime": (0, 255, 0),
		    "Navy": (0, 0, 128),
		    "Teal": (0, 128, 128),
		    "Maroon": (128, 0, 0),
		    "Olive": (128, 128, 0)
		}

		rgb = color_map.get(color_name, (0, 0, 0))
		color = (*rgb, int(255 * alpha / 100))
		return pg.mkBrush(color=color)

	def apply_processing(self, x, y):
		"""Apply smoothing and decimation to data"""
		if self.smooth_enabled.isChecked():
			method = self.smooth_method.currentText()
			window = self.smooth_window.value()

			# Ensure window is odd
			if window % 2 == 0:
				window += 1

			# Ensure window doesn't exceed data length
			if window > len(y):
				window = len(y) if len(y) % 2 == 1 else len(y) - 1

			if len(y) > window and window >= 3:
				if method == "Savitzky-Golay":
					if SCIPY_AVAILABLE:
						y = savgol_filter(y, window, min(3, window - 1))
					else:
						# Fallback to moving average
						y = pd.Series(y).rolling(window, center=True).mean().bfill().ffill().values
				elif method == "Gaussian":
					if SCIPY_AVAILABLE:
						y = gaussian_filter1d(y, window / 5)
					else:
						# Fallback to moving average
						y = pd.Series(y).rolling(window, center=True).mean().bfill().ffill().values
				elif method == "Moving Average":
					y = pd.Series(y).rolling(window, center=True).mean().bfill().ffill().values

		if self.decimate_enabled.isChecked():
			factor = self.decimate_factor.value()
			x = x[::factor]
			y = y[::factor]

		return x, y

	def get_data_for_column(self, col_name):
		"""Get x,y data for a column (handles comparison files)"""
		if '(' in col_name and col_name.endswith(')'):
			# Format: "column_name (file_name)"
			parts = col_name.rsplit(' (', 1)
			actual_col = parts[0]
			file_name = parts[1][:-1]  # Remove trailing )

			if file_name in self.loaded_files:
				df = self.loaded_files[file_name]
				if actual_col in df.columns:
					# Use first column as x or match main x column
					x_col = df.columns[0]
					return df[x_col].to_numpy(), df[actual_col].to_numpy()

		# Regular column from main dataframe
		x_col = self.x_selector.currentText()
		if col_name in self.df.columns:
			return self.df[x_col].to_numpy(), self.df[col_name].to_numpy()

		return None, None

	def plot_selected(self):
		if self.df is None:
			return

		x_col = self.x_selector.currentText()
		if not x_col:
			self.statusBar().showMessage("Select an X-axis column.")
			return

		# Get main X data (or from comparison file if selected)
		if '(' in x_col and x_col.endswith(')'):
			# X is from a comparison file
			x_data, _ = self.get_data_for_column(x_col)
			if x_data is None:
				self.statusBar().showMessage("Error loading X-axis data.")
				return
			x = x_data
		else:
			# X is from main dataframe
			x = self.df[x_col].to_numpy()

		self.main_plot.clear()
		self.right_view.clear()

		# Re-add analysis items
		for item in self.analysis_items:
			if isinstance(item, pg.PlotDataItem):
				self.main_plot.addItem(item)
			elif isinstance(item, pg.TextItem):
				self.main_plot.addItem(item)

		# Re-add reference lines
		for line in self.reference_lines:
			self.main_plot.addItem(line)

		# Re-add highlighted regions
		for region in self.highlighted_regions:
			self.main_plot.addItem(region)

		# Re-add legend and grid
		self.legend = self.main_plot.addLegend(offset=(10, 10))
		self.main_plot.showGrid(x=True, y=True, alpha=0.3)

		# Re-add crosshair
		self.main_plot.addItem(self.vLine, ignoreBounds=True)
		self.main_plot.addItem(self.hLine, ignoreBounds=True)

		self.settings.setValue("x_column", x_col)
		left_labels, right_labels = [], []
		left_cols, right_cols = [], []
		self.series_saved_styles.clear()

		for item in self.y1_list.selectedItems():
			y_col = item.text()
			style = self.series_style.get(y_col, {})

			# Check visibility
			visible_widget = style.get("visible")
			if visible_widget and not visible_widget.isChecked():
				continue

			left_labels.append(y_col)
			left_cols.append(y_col)

			# Use get_data_for_column to support comparison files
			x_data, y_data = self.get_data_for_column(y_col)
			if y_data is None:
				continue

			# Use the column's own x data if from comparison file, otherwise use main x
			if x_data is not None:
				x_plot_orig = x_data
			else:
				x_plot_orig = x

			x_plot, y_plot = self.apply_processing(x_plot_orig.copy(), y_data)

			# Extract style settings with proper defaults
			line_widget = style.get("line")
			marker_widget = style.get("marker")
			color_widget = style.get("color")
			width_widget = style.get("width")
			alpha_widget = style.get("alpha")

			line_style = line_widget.currentText() if line_widget else "Solid"
			marker = marker_widget.currentText() if marker_widget else "None"
			color = color_widget.currentText() if color_widget else ("White" if self.theme_dark else "Black")
			width = width_widget.value() if width_widget else 2.0
			alpha = alpha_widget.value() if alpha_widget else 100

			symbol = None if marker == "None" else marker
			self.series_saved_styles[y_col] = {
			    "line": line_style,
			    "marker": marker,
			    "color": color,
			    "width": width,
			    "alpha": alpha
			}
			marker_size = self.marker_size.value() if hasattr(self, 'marker_size') else 8

			pen = self.get_pen(line_style, color, width, alpha)
			brush = self.get_brush(color, alpha) if symbol else None

			self.main_plot.plot(
			    x_plot, y_plot, pen=pen, symbol=symbol, symbolSize=marker_size, symbolBrush=brush, name=y_col)

		for item in self.y2_list.selectedItems():
			y_col = item.text()
			style = self.series_style.get(y_col, {})

			# Check visibility
			visible_widget = style.get("visible")
			if visible_widget and not visible_widget.isChecked():
				continue

			right_labels.append(y_col)
			right_cols.append(y_col)

			# Use get_data_for_column to support comparison files
			x_data, y_data = self.get_data_for_column(y_col)
			if y_data is None:
				continue

			# Use the column's own x data if from comparison file, otherwise use main x
			if x_data is not None:
				x_plot_orig = x_data
			else:
				x_plot_orig = x

			x_plot, y_plot = self.apply_processing(x_plot_orig.copy(), y_data)

			# Extract style settings with proper defaults
			line_widget = style.get("line")
			marker_widget = style.get("marker")
			color_widget = style.get("color")
			width_widget = style.get("width")
			alpha_widget = style.get("alpha")

			line_style = line_widget.currentText() if line_widget else "Solid"
			marker = marker_widget.currentText() if marker_widget else "None"
			color = color_widget.currentText() if color_widget else ("White" if self.theme_dark else "Black")
			width = width_widget.value() if width_widget else 2.0
			alpha = alpha_widget.value() if alpha_widget else 100

			symbol = None if marker == "None" else marker
			self.series_saved_styles[y_col] = {
			    "line": line_style,
			    "marker": marker,
			    "color": color,
			    "width": width,
			    "alpha": alpha
			}
			marker_size = self.marker_size.value() if hasattr(self, 'marker_size') else 8

			pen = self.get_pen(line_style, color, width, alpha)
			brush = self.get_brush(color, alpha) if symbol else None

			curve = pg.PlotDataItem(x_plot, y_plot, pen=pen, symbol=symbol, symbolSize=marker_size, symbolBrush=brush)
			self.right_view.addItem(curve)

		self.settings.setValue("series_styles", self.series_saved_styles)
		self.settings.setValue("y1_columns", left_cols)
		self.settings.setValue("y2_columns", right_cols)

		# Set axis labels - use custom labels if provided, otherwise use column names
		x_label = self.x_axis_label if self.x_axis_label else x_col
		self.main_plot.setLabel('bottom', x_label, **{'font-size': '12pt'})

		if left_labels:
			y1_label = self.y1_axis_label if self.y1_axis_label else ', '.join(left_labels)
			self.main_plot.setLabel('left', y1_label, **{'font-size': '11pt'})
		else:
			self.main_plot.setLabel('left', '')

		if right_labels:
			y2_label = self.y2_axis_label if self.y2_axis_label else ', '.join(right_labels)
			self.main_plot.getAxis('right').setLabel(y2_label, **{'font-size': '11pt'})
		else:
			self.main_plot.getAxis('right').setLabel('')

		# Update analysis overlays
		if hasattr(self, 'update_analysis'):
			self.update_analysis()

	def toggle_zoom_mode(self, checked):
		self.zoom_mode = checked
		self.zoom_action.setText("🔍 Zoom ON" if checked else "🔍 Box Zoom")
		vb = self.main_plot.getViewBox()
		vb.setMouseMode(pg.ViewBox.RectMode if checked else pg.ViewBox.PanMode)

	def reset_zoom(self):
		self.main_plot.enableAutoRange(axis=pg.ViewBox.XYAxes)
		self.right_view.enableAutoRange(axis=pg.ViewBox.XYAxes)

	def check_file_update(self):
		if not self.csv_path or not os.path.exists(self.csv_path):
			return

		try:
			new_mtime = os.path.getmtime(self.csv_path)
		except FileNotFoundError:
			return

		if new_mtime != self.csv_mtime:
			self.csv_mtime = new_mtime
			try:
				new_df = pd.read_csv(self.csv_path, on_bad_lines='warn')
			except Exception as e:
				self.statusBar().showMessage(f"Failed to reload: {e}")
				return

			self.df = new_df
			x_range, y_range = self.main_plot.getViewBox().viewRange()
			self.plot_selected()
			self.main_plot.setXRange(*x_range, padding=0)
			self.main_plot.setYRange(*y_range, padding=0)
			self.update_csv_preview()
			self.update_statistics()
			self.statusBar().showMessage(f"File reloaded: {os.path.basename(self.csv_path)}")

	def init_csv_preview_dock(self):
		self.csv_preview = QTableWidget()
		self.csv_preview.setColumnCount(6)
		self.csv_preview.setHorizontalHeaderLabels(["Column", "Type", "Min", "Max", "Mean", "Std"])
		self.csv_preview.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)

		self.csv_dock = QDockWidget("CSV Column Summary", self)
		self.csv_dock.setWidget(self.csv_preview)
		self.csv_dock.setFloating(False)
		self.csv_dock.visibilityChanged.connect(self.on_dock_visibility_changed)
		self.addDockWidget(Qt.BottomDockWidgetArea, self.csv_dock)

	def init_statistics_dock(self):
		self.stats_text = QTextEdit()
		self.stats_text.setReadOnly(True)
		self.stats_text.setMaximumHeight(150)

		self.stats_dock = QDockWidget("Dataset Statistics", self)
		self.stats_dock.setWidget(self.stats_text)
		self.stats_dock.setFloating(False)
		self.stats_dock.visibilityChanged.connect(self.on_dock_visibility_changed)
		self.addDockWidget(Qt.BottomDockWidgetArea, self.stats_dock)

	def on_dock_visibility_changed(self):
		"""Keep menu checkboxes in sync with dock visibility"""
		if hasattr(self, 'toggle_summary_action'):
			self.toggle_summary_action.setChecked(self.csv_dock.isVisible())
		if hasattr(self, 'toggle_stats_action'):
			self.toggle_stats_action.setChecked(self.stats_dock.isVisible())

	def update_csv_preview(self):
		if self.df is None:
			return
		numeric_cols = [c for c in self.df.columns if pd.api.types.is_numeric_dtype(self.df[c])]
		self.csv_preview.setRowCount(len(numeric_cols))

		for i, col in enumerate(numeric_cols):
			col_type = str(self.df[col].dtype)
			min_val = self.df[col].min()
			max_val = self.df[col].max()
			mean_val = self.df[col].mean()
			std_val = self.df[col].std()

			self.csv_preview.setItem(i, 0, QTableWidgetItem(col))
			self.csv_preview.setItem(i, 1, QTableWidgetItem(col_type))
			self.csv_preview.setItem(i, 2, QTableWidgetItem(f"{min_val:.4g}"))
			self.csv_preview.setItem(i, 3, QTableWidgetItem(f"{max_val:.4g}"))
			self.csv_preview.setItem(i, 4, QTableWidgetItem(f"{mean_val:.4g}"))
			self.csv_preview.setItem(i, 5, QTableWidgetItem(f"{std_val:.4g}"))

	def update_statistics(self):
		if self.df is None:
			return

		stats = f"""
<b>Dataset Overview:</b><br>
• Total Rows: {len(self.df):,}<br>
• Total Columns: {len(self.df.columns)}<br>
• Numeric Columns: {len([c for c in self.df.columns if pd.api.types.is_numeric_dtype(self.df[c])])}<br>
• Memory Usage: {self.df.memory_usage(deep=True).sum() / 1024**2:.2f} MB<br>
• Missing Values: {self.df.isnull().sum().sum()}
        """
		self.stats_text.setHtml(stats)

	def save_plot_view(self):
		if not hasattr(self, 'main_plot'):
			return

		filename, _ = QFileDialog.getSaveFileName(
		    self, "Save Plot View", "", "PNG (*.png);;JPG (*.jpg);;SVG (*.svg);;PDF (*.pdf)")

		if filename:
			if filename.endswith('.svg'):
				exporter = pg.exporters.SVGExporter(self.main_plot)
			elif filename.endswith('.pdf'):
				exporter = pg.exporters.PDFExporter(self.main_plot)
			else:
				exporter = pg.exporters.ImageExporter(self.main_plot)
				exporter.parameters()['width'] = 3000

			exporter.export(filename)
			self.statusBar().showMessage(f"Plot saved to {filename}")

	def export_data(self):
		if self.df is None:
			return

		filename, _ = QFileDialog.getSaveFileName(self, "Export Data", "", "CSV (*.csv);;Excel (*.xlsx)")

		if filename:
			try:
				if filename.endswith('.xlsx'):
					self.df.to_excel(filename, index=False)
				else:
					self.df.to_csv(filename, index=False)
				self.statusBar().showMessage(f"Data exported to {filename}")
			except Exception as e:
				QMessageBox.critical(self, "Export Error", str(e))

	def add_annotation(self):
		if not hasattr(self, 'main_plot'):
			return

		dialog = QDialog(self)
		dialog.setWindowTitle("Add Annotation")
		layout = QFormLayout(dialog)

		text_input = QLineEdit()
		x_input = QDoubleSpinBox()
		x_input.setRange(-1e10, 1e10)
		y_input = QDoubleSpinBox()
		y_input.setRange(-1e10, 1e10)

		layout.addRow("Text:", text_input)
		layout.addRow("X Position:", x_input)
		layout.addRow("Y Position:", y_input)

		buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
		buttons.accepted.connect(dialog.accept)
		buttons.rejected.connect(dialog.reject)
		layout.addRow(buttons)

		if dialog.exec_() == QDialog.Accepted:
			text = pg.TextItem(text_input.text(), anchor=(0, 1))
			text.setPos(x_input.value(), y_input.value())
			self.main_plot.addItem(text)

	def clear_annotations(self):
		if hasattr(self, 'main_plot'):
			for item in self.main_plot.items[:]:
				if isinstance(item, pg.TextItem):
					self.main_plot.removeItem(item)

	def set_plot_title(self):
		"""Dialog to set custom plot title"""
		text, ok = QInputDialog.getText(self, "Set Plot Title", "Enter plot title:", QLineEdit.Normal, self.plot_title)
		if ok:
			self.plot_title = text
			self.settings.setValue("plot_title", self.plot_title)
			if hasattr(self, 'main_plot'):
				if text:
					self.main_plot.setTitle(text, size='12pt')
				else:
					self.main_plot.setTitle("")

	def set_axis_labels(self):
		"""Dialog to set custom axis labels"""
		dialog = QDialog(self)
		dialog.setWindowTitle("Set Axis Labels")
		layout = QFormLayout(dialog)

		x_input = QLineEdit(self.x_axis_label)
		x_input.setPlaceholderText("Auto (column name)")
		y1_input = QLineEdit(self.y1_axis_label)
		y1_input.setPlaceholderText("Auto (column names)")
		y2_input = QLineEdit(self.y2_axis_label)
		y2_input.setPlaceholderText("Auto (column names)")

		layout.addRow("X-Axis Label:", x_input)
		layout.addRow("Y-Axis Left Label:", y1_input)
		layout.addRow("Y-Axis Right Label:", y2_input)

		buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
		buttons.accepted.connect(dialog.accept)
		buttons.rejected.connect(dialog.reject)
		layout.addRow(buttons)

		if dialog.exec_() == QDialog.Accepted:
			self.x_axis_label = x_input.text()
			self.y1_axis_label = y1_input.text()
			self.y2_axis_label = y2_input.text()
			self.settings.setValue("x_axis_label", self.x_axis_label)
			self.settings.setValue("y1_axis_label", self.y1_axis_label)
			self.settings.setValue("y2_axis_label", self.y2_axis_label)
			self.plot_selected()

	def show_shortcuts(self):
		shortcuts = """
<b>Keyboard Shortcuts:</b><br><br>
<b>File Operations:</b><br>
• Ctrl+O: Open CSV<br>
• Ctrl+S: Save Plot<br>
• Ctrl+Q: Quit<br><br>
<b>View Controls:</b><br>
• C: Toggle Crosshair<br>
• R: Reset Zoom<br><br>
<b>Plot Operations:</b><br>
• Ctrl+P: Update Plot<br>
• Ctrl+D: Clear All Selections<br>
• Ctrl+T: Set Plot Title<br>
• Ctrl+L: Set Axis Labels<br>
• Ctrl+A: Add Annotation<br><br>
<b>Analysis:</b><br>
• Access via Analysis menu or Analysis tab<br>
• Curve fitting, peak detection, derivatives, FFT<br><br>
<b>Help:</b><br>
• F1: Show Shortcuts<br>
        """
		QMessageBox.information(self, "Keyboard Shortcuts", shortcuts)

	def show_about(self):
		scipy_status = "✓ Installed" if SCIPY_AVAILABLE else "✗ Not installed (advanced features unavailable)"
		about_text = f"""
<h3>CSV Dual-Axis Plot Viewer Pro</h3>
<p>Version 3.0</p>
<p>A professional tool for visualizing CSV data with dual Y-axes, 
advanced styling, data processing, and comprehensive analysis tools.</p>
<p><b>Core Features:</b></p>
<ul>
<li>Dual Y-axis plotting with independent scales</li>
<li>Advanced styling (colors, line styles, markers, opacity)</li>
<li>Data smoothing and decimation</li>
<li>Crosshair cursor with coordinates</li>
<li>Logarithmic scales</li>
</ul>
<p><b>Analysis Tools:</b></p>
<ul>
<li><b>Curve Fitting</b> - Linear, polynomial, exponential, logarithmic, power</li>
<li><b>Peak Detection</b> - Automatically find local maxima</li>
<li><b>Derivative Analysis</b> - Calculate and plot rate of change</li>
<li><b>FFT/Frequency Analysis</b> - Frequency spectrum analysis</li>
</ul>
<p><b>Enhanced Visualization:</b></p>
<ul>
<li><b>Data Value Tooltip</b> - Hover to see exact X,Y values</li>
<li><b>Adjustable Marker Size</b> - Control symbol size (3-30px)</li>
<li><b>Reference Lines</b> - Add movable vertical/horizontal markers</li>
<li><b>Region Highlighting</b> - Shade areas of interest with custom colors</li>
<li><b>Multiple CSV Comparison</b> - Overlay data from multiple files</li>
<li><b>Custom Grid Spacing</b> - Adjust grid intervals</li>
</ul>
<p><b>Other Features:</b></p>
<ul>
<li>Auto file monitoring and reload</li>
<li>Multiple export formats (PNG, SVG, PDF)</li>
<li>Drag-and-drop CSV loading</li>
<li>Custom plot titles and axis labels</li>
<li>Column search/filter</li>
<li>Recent columns tracking</li>
<li>Collapsible panels for maximizing plot area</li>
</ul>
<p><b>Dependencies:</b></p>
<ul>
<li>scipy: {scipy_status}</li>
</ul>
<p><i>To install scipy: pip install scipy</i></p>
<p><b>Tips:</b></p>
<ul>
<li>Go to Visualization tab for tooltips, reference lines, regions, and multi-file comparison</li>
<li>Use Analysis tab for curve fitting, peak detection, derivatives, and FFT</li>
<li>Hover over data to see exact values with tooltip enabled</li>
<li>Reference lines are draggable after creation</li>
<li>Add multiple CSV files to overlay and compare datasets</li>
</ul>
        """
		QMessageBox.about(self, "About", about_text)

if __name__ == "__main__":
	app = QApplication(sys.argv)
	app.setStyle('Fusion')
	window = CSVPlotter()
	window.show()
	sys.exit(app.exec_())
