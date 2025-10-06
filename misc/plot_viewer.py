import sys
import os
import numpy as np
import pandas as pd
import pyqtgraph as pg
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QFileDialog, QVBoxLayout, QWidget, QLabel, QComboBox, QListWidget, QPushButton,
    QListWidgetItem, QHBoxLayout, QSplitter, QCheckBox, QFormLayout, QGroupBox, QTableWidget, QTableWidgetItem,
    QDockWidget, QSizePolicy, QMenuBar, QAction, QToolBar, QSpinBox, QDoubleSpinBox, QScrollArea, QMessageBox,
    QLineEdit, QDialog, QDialogButtonBox, QTabWidget, QSlider, QTextEdit, QInputDialog)
from PyQt5.QtCore import Qt, QTimer, QSettings, QVariant, QMimeData
from PyQt5.QtGui import QKeySequence, QColor, QDragEnterEvent, QDropEvent
import pyqtgraph.exporters

# Optional scipy imports with fallbacks
try:
	from scipy.signal import savgol_filter
	from scipy.ndimage import gaussian_filter1d
	SCIPY_AVAILABLE = True
except ImportError:
	SCIPY_AVAILABLE = False
	print("Warning: scipy not available. Smoothing features will be limited.")

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

		# Processing Tab
		self.processing_tab = QWidget()
		self.processing_layout = QVBoxLayout(self.processing_tab)
		self.build_processing_controls()
		self.tabs.addTab(self.processing_tab, "Processing")

		self.splitter.setStretchFactor(0, 3)
		self.splitter.setStretchFactor(1, 1)

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

		# Theme toggle
		theme_layout = QHBoxLayout()
		self.theme_checkbox = QCheckBox("Dark Theme")
		self.theme_checkbox.setChecked(self.theme_dark)
		self.theme_checkbox.stateChanged.connect(self.toggle_theme_checkbox)
		theme_layout.addWidget(self.theme_checkbox)
		theme_layout.addStretch()
		self.data_layout.addLayout(theme_layout)

	def build_style_controls(self):
		self.style_scroll = QScrollArea()
		self.style_scroll.setWidgetResizable(True)
		self.style_widget = QWidget()
		self.style_form = QFormLayout(self.style_widget)
		self.style_scroll.setWidget(self.style_widget)
		self.style_layout.addWidget(self.style_scroll)

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
		colors = [
		    "Black", "Red", "Green", "Blue", "Magenta", "Cyan", "Yellow", "Orange", "Purple", "Brown", "Pink", "Lime",
		    "Navy", "Teal", "Maroon", "Olive"
		]

		def add_style_rows(label_prefix, items):
			for item in items:
				name = item.text()

				# Visibility toggle
				visible_check = QCheckBox("Visible")
				visible_check.setChecked(self.series_visibility.get(name, True))
				visible_check.stateChanged.connect(self.plot_selected)

				line_style = QComboBox()
				line_style.addItems(["Solid", "Dashed", "Dotted"])

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

				if name in self.series_saved_styles:
					style = self.series_saved_styles[name]
					line_style.setCurrentText(style.get("line", "Solid"))
					marker_style.setCurrentText(style.get("marker", "None"))
					color_style.setCurrentText(style.get("color", "Black"))
					line_width.setValue(style.get("width", 2.0))
					alpha_slider.setValue(style.get("alpha", 100))

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

	def toggle_theme_checkbox(self):
		self.theme_dark = self.theme_checkbox.isChecked()
		self.dark_theme_action.setChecked(self.theme_dark)
		self.settings.setValue("theme_dark", self.theme_dark)
		self.replace_plot_widget()

	def toggle_crosshair(self):
		self.crosshair_enabled = self.crosshair_action.isChecked()
		self.vLine.setVisible(self.crosshair_enabled)
		self.hLine.setVisible(self.crosshair_enabled)

	def mouse_moved(self, evt):
		if not self.crosshair_enabled or self.df is None:
			return
		pos = evt[0]
		if self.main_plot.sceneBoundingRect().contains(pos):
			mousePoint = self.main_plot.vb.mapSceneToView(pos)
			self.vLine.setPos(mousePoint.x())
			self.hLine.setPos(mousePoint.y())
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
		if self.df is not None:
			for col in self.df.columns:
				if pd.api.types.is_numeric_dtype(self.df[col]):
					self.x_selector.addItem(col)
					self.y1_list.addItem(QListWidgetItem(col))
					self.y2_list.addItem(QListWidgetItem(col))

	def get_pen(self, style_name, color_name, width=2.0, alpha=100):
		pen_styles = {"Solid": Qt.SolidLine, "Dashed": Qt.DashLine, "Dotted": Qt.DotLine}
		color_map = {
		    "Black": 'k',
		    "Red": 'r',
		    "Green": 'g',
		    "Blue": 'b',
		    "Magenta": 'm',
		    "Cyan": 'c',
		    "Yellow": 'y',
		    "Gray": 'gray',
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

		color = color_map.get(color_name, 'k')
		if isinstance(color, tuple):
			color = (*color, int(255 * alpha / 100))

		return pg.mkPen(color=color, width=width, style=pen_styles.get(style_name, Qt.SolidLine))

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

	def plot_selected(self):
		if self.df is None:
			return

		x_col = self.x_selector.currentText()
		if not x_col:
			self.statusBar().showMessage("Select an X-axis column.")
			return

		x = self.df[x_col].to_numpy()
		self.main_plot.clear()
		self.right_view.clear()

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

			if not style.get("visible", QCheckBox()).isChecked():
				continue

			left_labels.append(y_col)
			left_cols.append(y_col)
			y = self.df[y_col].to_numpy()

			x_plot, y_plot = self.apply_processing(x.copy(), y)

			line_style = style.get("line", QComboBox()).currentText()
			marker = style.get("marker", QComboBox()).currentText()
			color = style.get("color", QComboBox()).currentText()
			width = style.get("width", QDoubleSpinBox()).value()
			alpha = style.get("alpha", QSlider()).value()

			symbol = None if marker == "None" else marker
			self.series_saved_styles[y_col] = {
			    "line": line_style,
			    "marker": marker,
			    "color": color,
			    "width": width,
			    "alpha": alpha
			}
			self.main_plot.plot(
			    x_plot,
			    y_plot,
			    pen=self.get_pen(line_style, color, width, alpha),
			    symbol=symbol,
			    symbolSize=8,
			    name=y_col)

		for item in self.y2_list.selectedItems():
			y_col = item.text()
			style = self.series_style.get(y_col, {})

			if not style.get("visible", QCheckBox()).isChecked():
				continue

			right_labels.append(y_col)
			right_cols.append(y_col)
			y = self.df[y_col].to_numpy()

			x_plot, y_plot = self.apply_processing(x.copy(), y)

			line_style = style.get("line", QComboBox()).currentText()
			marker = style.get("marker", QComboBox()).currentText()
			color = style.get("color", QComboBox()).currentText()
			width = style.get("width", QDoubleSpinBox()).value()
			alpha = style.get("alpha", QSlider()).value()

			symbol = None if marker == "None" else marker
			self.series_saved_styles[y_col] = {
			    "line": line_style,
			    "marker": marker,
			    "color": color,
			    "width": width,
			    "alpha": alpha
			}
			curve = pg.PlotDataItem(
			    x_plot, y_plot, pen=self.get_pen(line_style, color, width, alpha), symbol=symbol, symbolSize=8)
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
<b>Help:</b><br>
• F1: Show Shortcuts<br>
        """
		QMessageBox.information(self, "Keyboard Shortcuts", shortcuts)

	def show_about(self):
		scipy_status = "✓ Installed" if SCIPY_AVAILABLE else "✗ Not installed (advanced smoothing unavailable)"
		about_text = f"""
<h3>CSV Dual-Axis Plot Viewer Pro</h3>
<p>Version 2.1</p>
<p>A professional tool for visualizing CSV data with dual Y-axes, 
advanced styling, data processing, and real-time monitoring.</p>
<p><b>Features:</b></p>
<ul>
<li>Dual Y-axis plotting with independent scales</li>
<li>Advanced styling (colors, line styles, markers, opacity)</li>
<li>Data smoothing and decimation</li>
<li>Crosshair cursor with coordinates</li>
<li>Logarithmic scales</li>
<li>Auto file monitoring and reload</li>
<li>Multiple export formats (PNG, SVG, PDF)</li>
<li>Drag-and-drop CSV loading</li>
<li><b>Custom plot titles and axis labels</b></li>
<li><b>Column search/filter</b></li>
<li><b>Recent columns tracking</b></li>
<li><b>Collapsible panels</b> for maximizing plot area</li>
</ul>
<p><b>Dependencies:</b></p>
<ul>
<li>scipy: {scipy_status}</li>
</ul>
<p><i>To install scipy: pip install scipy</i></p>
<p><b>Tips:</b></p>
<ul>
<li>Use Ctrl+T to set a plot title</li>
<li>Use Ctrl+L to customize axis labels</li>
<li>Toggle panels via View menu to maximize plot</li>
<li>Search columns to quickly find what you need</li>
<li>Press C for crosshair, R to reset zoom</li>
</ul>
        """
		QMessageBox.about(self, "About", about_text)

if __name__ == "__main__":
	app = QApplication(sys.argv)
	app.setStyle('Fusion')
	window = CSVPlotter()
	window.show()
	sys.exit(app.exec_())
