import sys
import os
import time
import pandas as pd
import pyqtgraph as pg
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QFileDialog, QVBoxLayout, QWidget, QLabel, QComboBox, QListWidget, QPushButton,
    QListWidgetItem, QHBoxLayout, QSplitter, QCheckBox, QFormLayout, QGroupBox, QToolBox, QTableWidget,
    QTableWidgetItem, QDockWidget, QSizePolicy)
from PyQt5.QtCore import Qt, QTimer, QSettings, QVariant
import pyqtgraph.exporters

class CSVPlotter(QMainWindow):

	def __init__(self):
		super().__init__()
		self.setWindowTitle("CSV Dual-Axis Plot Viewer")
		self.resize(1200, 800)

		self.settings = QSettings("XFlow", "CSVPlotter")
		self.theme_dark = self.settings.value("theme_dark", True, type=bool)
		self.series_saved_styles = self.settings.value("series_styles", {}, type=dict)

		self.df = None
		self.csv_path = None
		self.csv_mtime = None
		self.zoom_mode = False

		self.central_widget = QWidget()
		self.setCentralWidget(self.central_widget)
		self.main_layout = QVBoxLayout(self.central_widget)

		self.setup_controls()

		self.splitter = QSplitter(Qt.Horizontal)
		self.main_layout.addLayout(self.top_controls)
		self.main_layout.addLayout(self.theme_controls)
		self.main_layout.addWidget(self.splitter)

		self.plot_area = QWidget()
		self.splitter.addWidget(self.plot_area)

		self.control_panel = QWidget()
		self.control_layout = QVBoxLayout(self.control_panel)
		self.splitter.addWidget(self.control_panel)

		self.build_control_panel()
		self.replace_plot_widget()

		self.monitor_timer = QTimer(self)
		self.monitor_timer.setInterval(2000)
		self.monitor_timer.timeout.connect(self.check_file_update)
		self.monitor_timer.start()

		self.init_csv_preview_dock()

		last_file = self.settings.value("last_csv_file", "", type=str)
		if last_file and os.path.isfile(last_file):
			self.load_csv(last_file)

	def apply_theme(self):
		if self.theme_dark:
			pg.setConfigOption('background', 'k')
			pg.setConfigOption('foreground', 'w')
		else:
			pg.setConfigOption('background', 'w')
			pg.setConfigOption('foreground', 'k')

	def setup_controls(self):
		self.top_controls = QHBoxLayout()
		self.select_button = QPushButton("Open CSV")
		self.select_button.clicked.connect(self.open_file_dialog)
		self.zoom_button = QPushButton("Enable Box Zoom")
		self.zoom_button.setCheckable(True)
		self.zoom_button.toggled.connect(self.toggle_zoom_mode)
		self.autoscale_button = QPushButton("Reset Zoom")
		self.autoscale_button.clicked.connect(self.reset_zoom)
		self.plot_button = QPushButton("Plot Selected Columns")
		self.plot_button.clicked.connect(self.plot_selected)
		self.save_button = QPushButton("Save Plot View")
		self.save_button.clicked.connect(self.save_plot_view)

		self.top_controls.addWidget(self.select_button)
		self.top_controls.addWidget(self.zoom_button)
		self.top_controls.addWidget(self.autoscale_button)
		self.top_controls.addWidget(self.plot_button)
		self.top_controls.addWidget(self.save_button)
		self.top_controls.addStretch()

		self.theme_controls = QHBoxLayout()
		self.theme_toggle = QCheckBox("Dark Theme")
		self.theme_toggle.setChecked(self.theme_dark)
		self.theme_toggle.stateChanged.connect(self.toggle_theme)
		self.theme_controls.addWidget(self.theme_toggle)
		self.theme_controls.addStretch()

	def build_control_panel(self):
		self.control_layout.addWidget(QLabel("X-Axis:"))
		self.x_selector = QComboBox()
		self.control_layout.addWidget(self.x_selector)

		self.control_layout.addWidget(QLabel("Y-Axis (Left):"))
		self.y1_list = QListWidget()
		self.y1_list.setSelectionMode(QListWidget.MultiSelection)
		self.y1_list.itemSelectionChanged.connect(self.update_style_selectors)
		self.control_layout.addWidget(self.y1_list)

		self.control_layout.addWidget(QLabel("Y-Axis (Right):"))
		self.y2_list = QListWidget()
		self.y2_list.setSelectionMode(QListWidget.MultiSelection)
		self.y2_list.itemSelectionChanged.connect(self.update_style_selectors)
		self.control_layout.addWidget(self.y2_list)

		from PyQt5.QtWidgets import QScrollArea

		self.style_scroll = QScrollArea()
		self.style_scroll.setWidgetResizable(True)
		self.style_widget = QWidget()
		self.style_layout = QFormLayout(self.style_widget)
		self.style_scroll.setWidget(self.style_widget)
		self.control_layout.addWidget(QLabel("Per-Series Style Options"))
		self.control_layout.addWidget(self.style_scroll)

		self.control_layout.addStretch()

	def update_style_selectors(self):
		while self.style_layout.count():
			item = self.style_layout.takeAt(0)
			if item.widget():
				item.widget().deleteLater()

		self.series_style = {}
		colors = ["Black", "Red", "Green", "Blue", "Magenta", "Cyan", "Yellow", "Gray"]

		def add_style_rows(label_prefix, items):
			for item in items:
				name = item.text()
				line_style = QComboBox()
				line_style.addItems(["Solid", "Dashed", "Dotted"])
				marker_style = QComboBox()
				marker_style.addItems(["None", "x", "o"])
				color_style = QComboBox()
				color_style.addItems(colors)

				if name in self.series_saved_styles:
					style = self.series_saved_styles[name]
					line_style.setCurrentText(style.get("line", "Solid"))
					marker_style.setCurrentText(style.get("marker", "None"))
					color_style.setCurrentText(style.get("color", "Black"))

				self.series_style[name] = {"line": line_style, "marker": marker_style, "color": color_style}

				group = QGroupBox(f"{label_prefix}: {name}")
				group_layout = QFormLayout(group)
				group_layout.addRow("Line", line_style)
				group_layout.addRow("Marker", marker_style)
				group_layout.addRow("Color", color_style)

				self.style_layout.addRow(group)

		add_style_rows("Left", self.y1_list.selectedItems())
		add_style_rows("Right", self.y2_list.selectedItems())

	def restore_selections(self):
		saved_x = self.settings.value("x_column", "", type=str)
		saved_y1 = self.settings.value("y1_columns", [], type=list)
		saved_y2 = self.settings.value("y2_columns", [], type=list)
		self.x_selector.setCurrentText(saved_x)

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

		self.plot_area.setParent(None)
		self.plot_area = pg.GraphicsLayoutWidget()
		self.main_plot = self.plot_area.addPlot()
		self.main_plot.showGrid(x=True, y=True)
		self.main_plot.setLabel('bottom', '')
		self.main_plot.setLabel('left', '')
		self.main_plot.addLegend()

		self.right_view = pg.ViewBox()
		self.main_plot.showAxis('right')
		self.main_plot.scene().addItem(self.right_view)
		self.main_plot.getAxis('right').linkToView(self.right_view)
		self.right_view.setXLink(self.main_plot)
		self.main_plot.getViewBox().sigResized.connect(
		    lambda: self.right_view.setGeometry(self.main_plot.getViewBox().sceneBoundingRect()))

		self.splitter.insertWidget(0, self.plot_area)

		if self.df is not None:
			self.load_column_selectors()
			self.restore_selections()
			self.plot_selected()
			if zoom_x:
				self.main_plot.setXRange(*zoom_x, padding=0)
			if zoom_y:
				self.main_plot.setYRange(*zoom_y, padding=0)

	def toggle_theme(self, state):
		self.theme_dark = (state == Qt.Checked)
		self.settings.setValue("theme_dark", QVariant(self.theme_dark))
		self.replace_plot_widget()

	def open_file_dialog(self):
		start_dir = self.settings.value("last_csv_dir", os.path.expanduser("~"))
		filename, _ = QFileDialog.getOpenFileName(self, "Open CSV File", start_dir, "CSV Files (*.csv)")
		if filename:
			self.load_csv(filename)

	def load_csv(self, filename):
		try:
			self.df = pd.read_csv(filename, on_bad_lines='warn')
		except Exception as e:
			self.statusBar().showMessage(f"Failed to read CSV: {e}")
			return

		self.statusBar().showMessage(f"Loaded: {filename}")
		self.csv_path = filename
		self.csv_mtime = os.path.getmtime(filename)
		self.settings.setValue("last_csv_file", filename)
		self.settings.setValue("last_csv_dir", os.path.dirname(filename))
		self.load_column_selectors()
		self.restore_selections()
		self.plot_selected()
		self.update_csv_preview()

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

	def get_pen(self, style_name, color_name):
		pen_styles = {"Solid": Qt.SolidLine, "Dashed": Qt.DashLine, "Dotted": Qt.DotLine}
		color_map = {
		    "Black": 'k',
		    "Red": 'r',
		    "Green": 'g',
		    "Blue": 'b',
		    "Magenta": 'm',
		    "Cyan": 'c',
		    "Yellow": 'y',
		    "Gray": 'gray'
		}
		return pg.mkPen(color=color_map.get(color_name, 'k'), width=2, style=pen_styles.get(style_name, Qt.SolidLine))

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
		self.main_plot.addLegend()
		self.main_plot.showGrid(x=True, y=True)

		self.settings.setValue("x_column", x_col)
		left_labels, right_labels = [], []
		left_cols, right_cols = [], []
		self.series_saved_styles.clear()

		for item in self.y1_list.selectedItems():
			y_col = item.text()
			left_labels.append(y_col)
			left_cols.append(y_col)
			y = self.df[y_col].to_numpy()
			style = self.series_style.get(y_col, {})
			line_style = style.get("line", QComboBox()).currentText()
			marker = style.get("marker", QComboBox()).currentText()
			color = style.get("color", QComboBox()).currentText()
			symbol = None if marker == "None" else marker
			self.series_saved_styles[y_col] = {"line": line_style, "marker": marker, "color": color}
			self.main_plot.plot(x, y, pen=self.get_pen(line_style, color), symbol=symbol, name=y_col)

		for item in self.y2_list.selectedItems():
			y_col = item.text()
			right_labels.append(y_col)
			right_cols.append(y_col)
			y = self.df[y_col].to_numpy()
			style = self.series_style.get(y_col, {})
			line_style = style.get("line", QComboBox()).currentText()
			marker = style.get("marker", QComboBox()).currentText()
			color = style.get("color", QComboBox()).currentText()
			symbol = None if marker == "None" else marker
			self.series_saved_styles[y_col] = {"line": line_style, "marker": marker, "color": color}
			curve = pg.PlotDataItem(x, y, pen=self.get_pen(line_style, color), symbol=symbol)
			self.right_view.addItem(curve)

		self.settings.setValue("series_styles", self.series_saved_styles)
		self.settings.setValue("y1_columns", left_cols)
		self.settings.setValue("y2_columns", right_cols)

		self.main_plot.setLabel('bottom', x_col)

		if left_labels:
			first_left = left_labels[0]
			first_left_color = self.series_saved_styles[first_left]["color"]
			self.main_plot.setLabel('left', ', '.join(left_labels))
			self.main_plot.getAxis('left').setTextPen(pg.mkPen(first_left_color))
		else:
			self.main_plot.setLabel('left', '')
			self.main_plot.getAxis('left').setTextPen(pg.mkPen('k' if not self.theme_dark else 'w'))

		if right_labels:
			first_right = right_labels[0]
			first_right_color = self.series_saved_styles[first_right]["color"]
			self.main_plot.getAxis('right').setLabel(', '.join(right_labels))
			self.main_plot.getAxis('right').setTextPen(pg.mkPen(first_right_color))
		else:
			self.main_plot.getAxis('right').setLabel('')
			self.main_plot.getAxis('right').setTextPen(pg.mkPen('k' if not self.theme_dark else 'w'))

	def toggle_zoom_mode(self, checked):
		self.zoom_mode = checked
		self.zoom_button.setText("Box Zoom ON" if checked else "Enable Box Zoom")
		vb = self.main_plot.getViewBox()
		vb.setMouseMode(pg.ViewBox.RectMode if checked else pg.ViewBox.PanMode)

	def reset_zoom(self):
		self.main_plot.enableAutoRange(axis=pg.ViewBox.XYAxes)
		self.right_view.enableAutoRange(axis=pg.ViewBox.XYAxes)

	def check_file_update(self):
		if not self.csv_path:
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
				self.statusBar().showMessage(f"Failed to reload CSV: {e}")
				return

			self.df = new_df
			x_range, y_range = self.main_plot.getViewBox().viewRange()
			self.plot_selected()
			self.main_plot.setXRange(*x_range, padding=0)
			self.main_plot.setYRange(*y_range, padding=0)
			self.update_csv_preview()

	def init_csv_preview_dock(self):
		self.csv_preview = QTableWidget()
		self.csv_preview.setColumnCount(4)
		self.csv_preview.setHorizontalHeaderLabels(["Column", "Type", "Min", "Max"])
		self.csv_preview.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)

		self.csv_dock = QDockWidget("CSV Column Summary", self)
		self.csv_dock.setWidget(self.csv_preview)
		self.csv_dock.setFloating(False)
		self.addDockWidget(Qt.BottomDockWidgetArea, self.csv_dock)

	def update_csv_preview(self):
		if self.df is None:
			return
		numeric_cols = [c for c in self.df.columns if pd.api.types.is_numeric_dtype(self.df[c])]
		self.csv_preview.setRowCount(len(numeric_cols))
		for i, col in enumerate(numeric_cols):
			col_type = str(self.df[col].dtype)
			min_val = self.df[col].min()
			max_val = self.df[col].max()
			self.csv_preview.setItem(i, 0, QTableWidgetItem(col))
			self.csv_preview.setItem(i, 1, QTableWidgetItem(col_type))
			self.csv_preview.setItem(i, 2, QTableWidgetItem(str(min_val)))
			self.csv_preview.setItem(i, 3, QTableWidgetItem(str(max_val)))

	def save_plot_view(self):
		if not hasattr(self, 'main_plot'):
			return
		filename, _ = QFileDialog.getSaveFileName(self, "Save Plot View", "", "PNG (*.png);;JPG (*.jpg);;BMP (*.bmp)")
		if filename:
			exporter = pg.exporters.ImageExporter(self.main_plot)
			exporter.parameters()['width'] = 2400  # Higher pixel density
			exporter.export(filename)
			self.statusBar().showMessage(f"Plot saved to {filename}")

if __name__ == "__main__":
	app = QApplication(sys.argv)
	window = CSVPlotter()
	window.show()
	sys.exit(app.exec_())
