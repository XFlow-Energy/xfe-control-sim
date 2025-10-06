import tkinter as tk
from tkinter import ttk, messagebox
import csv
import os
import subprocess
import shutil
import re

# Paths for CSV and config files (adjust as needed)
CSV_FILE = "/Users/jason/Documents/GitHub/xfe-control-sim/src/config/control_1_config.csv"
CONFIG_CMAKE_FILE = "/Users/jason/Documents/GitHub/xfe-control-sim/src/config/config.cmake"

def create_sample_csv():
	# Create a sample CSV if it doesn't exist
	if not os.path.exists(CSV_FILE):
		with open(CSV_FILE, mode="w", newline="") as file:
			writer = csv.DictWriter(file, fieldnames=["Name", "Age", "City"])
			writer.writeheader()
			writer.writerows(
			    [
			        {
			            "Name": "Alice",
			            "Age": "30",
			            "City": "New York"
			        }, {
			            "Name": "Bob",
			            "Age": "25",
			            "City": "Los Angeles"
			        }, {
			            "Name": "Charlie",
			            "Age": "35",
			            "City": "Chicago"
			        }
			    ])

class CSVEditorApp:

	def __init__(self, root):
		self.root = root
		self.root.title("Real-Time CSV Editor")
		self.last_mod_time = None
		self.data = []
		self.headers = []
		# Load CSV to get headers and data before creating widgets
		self.load_csv(refresh=False)
		self.create_widgets()
		self.refresh_treeview()
		self.poll_csv_changes()

	def create_widgets(self):
		# Create a Treeview using dynamic headers from the CSV file
		self.tree = ttk.Treeview(self.root, columns=self.headers, show="headings")
		for header in self.headers:
			self.tree.heading(header, text=header)
		self.tree.bind("<<TreeviewSelect>>", self.on_row_select)
		self.tree.pack(fill=tk.BOTH, expand=True, pady=10)

		# Create dynamic editor fields based on the CSV headers
		editor_frame = tk.Frame(self.root)
		editor_frame.pack(fill=tk.X, padx=10)

		self.entry_vars = {}
		for idx, header in enumerate(self.headers):
			tk.Label(editor_frame, text=f"{header}:").grid(row=idx, column=0, pady=5)
			var = tk.StringVar()
			entry = tk.Entry(editor_frame, textvariable=var)
			entry.grid(row=idx, column=1, pady=5, padx=5)
			self.entry_vars[header] = var

		self.update_button = tk.Button(editor_frame, text="Update Selected Row", command=self.update_row)
		self.update_button.grid(row=len(self.headers), column=0, columnspan=2, pady=10)

		# Additional Buttons for launching external programs and editing config
		button_frame = tk.Frame(self.root)
		button_frame.pack(fill=tk.X, padx=10, pady=10)

		# Checkbox to force recompile
		self.force_recompile = tk.IntVar(value=0)
		recompile_cb = tk.Checkbutton(button_frame, text="Force Recompile", variable=self.force_recompile)
		recompile_cb.pack(side=tk.LEFT, padx=5)

		compile_button = tk.Button(button_frame, text="Compile & Build", command=self.compile_program)
		compile_button.pack(side=tk.LEFT, padx=5)

		run_button = tk.Button(button_frame, text="Run Program", command=self.run_program)
		run_button.pack(side=tk.LEFT, padx=5)

		# Button to launch the config editor window
		edit_config_button = tk.Button(button_frame, text="Edit Config", command=self.edit_config_window)
		edit_config_button.pack(side=tk.LEFT, padx=5)

	def load_csv(self, refresh=True):
		# Load data from CSV and get headers
		try:
			with open(CSV_FILE, mode="r", newline="") as file:
				reader = csv.DictReader(file)
				self.data = list(reader)
				# Use the CSV file's header row to define headers
				self.headers = reader.fieldnames if reader.fieldnames else []
			self.last_mod_time = os.path.getmtime(CSV_FILE)
			if refresh and hasattr(self, "tree"):
				self.refresh_treeview()
		except Exception as e:
			messagebox.showerror("Error", f"Failed to load CSV: {e}")

	def refresh_treeview(self):
		# Clear current items in the Treeview
		for item in self.tree.get_children():
			self.tree.delete(item)
		# Insert new data using dynamic header order
		for row in self.data:
			values = tuple(row.get(header, "") for header in self.headers)
			self.tree.insert("", tk.END, values=values)

	def on_row_select(self, event):
		# When a row is selected, load its data into the editor fields
		selected_item = self.tree.focus()
		if not selected_item:
			return
		values = self.tree.item(selected_item, "values")
		for idx, header in enumerate(self.headers):
			self.entry_vars[header].set(values[idx])

	def update_row(self):
		# Get the selected row index
		selected_item = self.tree.focus()
		if not selected_item:
			messagebox.showwarning("Selection Error", "No row selected.")
			return
		selected_index = self.tree.index(selected_item)

		# Build new row data using dynamic headers
		new_row = {header: self.entry_vars[header].get() for header in self.headers}
		self.data[selected_index] = new_row

		# Write updated data back to the CSV file using dynamic headers
		try:
			with open(CSV_FILE, mode="w", newline="") as file:
				writer = csv.DictWriter(file, fieldnames=self.headers)
				writer.writeheader()
				writer.writerows(self.data)
			messagebox.showinfo("Success", "Row updated successfully!")
			self.refresh_treeview()
		except Exception as e:
			messagebox.showerror("Error", f"Failed to update CSV: {e}")

	def poll_csv_changes(self):
		# Check if the CSV file has been modified externally
		try:
			current_mod_time = os.path.getmtime(CSV_FILE)
			if current_mod_time != self.last_mod_time:
				self.load_csv()
		except Exception as e:
			print(f"Polling error: {e}")
		# Poll every 2 seconds
		self.root.after(2000, self.poll_csv_changes)

	# --- Helper functions for launching external programs ---
	def get_aero_controller_dir(self):
		# Get the top-level directory using git
		try:
			result = subprocess.run(
			    ["git", "rev-parse", "--show-toplevel"],
			    stdout=subprocess.PIPE,
			    stderr=subprocess.PIPE,
			    universal_newlines=True,
			    check=True)
			return result.stdout.strip()
		except Exception as e:
			messagebox.showerror("Error", f"Error obtaining top-level directory: {e}")
			return None

	def compile_program(self):
		# Kill any running aero_control instances
		try:
			subprocess.run(["killall", "-9", "-v", "aero_control"], check=True)
		except Exception as e:
			# Ignore errors if no instance is running
			print("Kill command error:", e)

		top_dir = self.get_aero_controller_dir()
		if not top_dir:
			return
		build_dir = os.path.join(top_dir, "build")

		# If force recompile is checked, remove the build directory if it exists
		if self.force_recompile.get() == 1 and os.path.exists(build_dir):
			try:
				shutil.rmtree(build_dir)
			except Exception as e:
				messagebox.showerror("Error", f"Failed to remove build directory: {e}")
				return
		# Create the build directory if it doesn't exist
		if not os.path.exists(build_dir):
			try:
				os.mkdir(build_dir)
			except Exception as e:
				messagebox.showerror("Error", f"Failed to create build directory: {e}")
				return

		# Change working directory to the build directory
		try:
			os.chdir(build_dir)
		except Exception as e:
			messagebox.showerror("Error", f"Failed to change directory: {e}")
			return

		# Create a Toplevel window for build output
		cmd_window = tk.Toplevel(self.root)
		cmd_window.title("Build Output")
		text_widget = tk.Text(cmd_window, wrap="word")
		text_widget.pack(fill=tk.BOTH, expand=True)

		# Define the sequential commands for building
		cmake_cmd = ["cmake", "-DCMAKE_BUILD_TYPE=Release", "../src"]
		nproc = str(os.cpu_count())
		make_cmd = ["make", "-j" + nproc]
		commands = [("cmake", cmake_cmd), ("make", make_cmd)]

		def run_commands_seq(commands, index=0):
			if index >= len(commands):
				text_widget.insert(tk.END, "\nBuild complete.\n")
				messagebox.showinfo("Build Complete", "Compilation and build completed successfully.")
				return
			cmd_name, cmd = commands[index]
			text_widget.insert(tk.END, f"\nRunning {cmd_name} command: {' '.join(cmd)}\n")
			proc = subprocess.Popen(
			    cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, cwd=build_dir)

			def poll():
				line = proc.stdout.readline()
				if line:
					text_widget.insert(tk.END, line)
					text_widget.see(tk.END)
				if proc.poll() is None:
					text_widget.after(100, poll)
				else:
					remaining = proc.stdout.read()
					if remaining:
						text_widget.insert(tk.END, remaining)
						text_widget.see(tk.END)
					if proc.returncode != 0:
						messagebox.showerror("Error", f"{cmd_name} command failed with return code {proc.returncode}")
						return
					else:
						text_widget.insert(tk.END, f"\n{cmd_name} completed successfully.\n")
						run_commands_seq(commands, index + 1)

			poll()

		run_commands_seq(commands)

	def run_program(self):
		top_dir = self.get_aero_controller_dir()
		if not top_dir:
			return
		exec_dir = os.path.join(top_dir, "build", "executables-out")
		if not os.path.exists(exec_dir):
			messagebox.showerror("Error", "Build directory or executables-out folder is missing. Compile first?")
			return
		exe_path = os.path.join(exec_dir, "aero_control")
		if not os.path.exists(exe_path):
			messagebox.showerror("Error", f"Executable not found: {exe_path}")
			return
		# Create a Toplevel window for the command output
		cmd_window = tk.Toplevel(self.root)
		cmd_window.title("Aero Control Output")
		text_widget = tk.Text(cmd_window, wrap="word")
		text_widget.pack(fill=tk.BOTH, expand=True)
		# Start the external process and capture its output
		self.proc = subprocess.Popen(
		    [exe_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True, cwd=exec_dir)
		# Start polling the process output
		self.poll_output(text_widget)

	def poll_output(self, text_widget):
		# Poll for new output from the aero_control process
		line = self.proc.stdout.readline()
		if line:
			text_widget.insert(tk.END, line)
			text_widget.see(tk.END)
		# If the process is still running, schedule another poll in 100ms
		if self.proc.poll() is None:
			text_widget.after(100, lambda: self.poll_output(text_widget))
		else:
			# Process has terminated; read any remaining output
			remaining = self.proc.stdout.read()
			if remaining:
				text_widget.insert(tk.END, remaining)
				text_widget.see(tk.END)

	# --- Methods for reading and updating config.cmake ---
	def parse_config_file(self):
		# Parse config.cmake and return a dictionary of key-value pairs for both set and option lines
		config = {}
		try:
			with open(CONFIG_CMAKE_FILE, "r") as f:
				for line in f:
					# Match set(...) lines (with or without CACHE)
					m = re.match(r'\s*set\(\s*(\w+)\s+"([^"]+)"(?:\s+CACHE\s+\w+\s+"[^"]+")?\s*\)', line)
					if m:
						key = m.group(1)
						val = m.group(2)
						config[key] = val
					else:
						# Match option(...) lines (ON/OFF)
						m2 = re.match(r'\s*option\(\s*(\w+)\s+"[^"]+"\s+(ON|OFF)\s*\)', line)
						if m2:
							key = m2.group(1)
							config[key] = m2.group(2)
		except Exception as e:
			messagebox.showerror("Error", f"Error reading config file: {e}")
		return config

	def update_config_file(self, new_values):
		# Read current file lines and update lines for specified keys using regex for both set and option
		with open(CONFIG_CMAKE_FILE, "r") as f:
			lines = f.readlines()
		new_lines = []
		for line in lines:
			# Check for set(...) lines
			m = re.match(r'(\s*set\(\s*(\w+)\s+)"[^"]+"((?:\s+CACHE\s+\w+\s+"[^"]+")?\s*\).*)', line)
			if m:
				key = m.group(2)
				if key in new_values:
					new_line = f'{m.group(1)}"{new_values[key]}"{m.group(3)}\n'
					new_lines.append(new_line)
					continue
			# Check for option(...) lines
			m2 = re.match(r'(\s*option\(\s*(\w+)\s+"[^"]+"\s+)(ON|OFF)(\s*\).*)', line)
			if m2:
				key = m2.group(2)
				if key in new_values:
					new_line = f'{m2.group(1)}{new_values[key]}{m2.group(4)}\n'
					new_lines.append(new_line)
					continue
			# For lines that don't match either pattern, just keep the line as is
			new_lines.append(line)
		with open(CONFIG_CMAKE_FILE, "w") as f:
			f.writelines(new_lines)

	def edit_config_window(self):
		# If a config editor window is already open, destroy it.
		if hasattr(self, 'config_window') and self.config_window.winfo_exists():
			self.config_window.destroy()

		self.config_window = tk.Toplevel(self.root)
		self.config_window.title("Edit config.cmake")
		editable_vars = [
		    "SYSTEM_CONFIG_FILENAME", "DRIVETRAIN_FILENAME", "NUMERICAL_INTEGRATOR_FILENAME", "EOM_FILENAME",
		    "FLOW_GEN_FILENAME", "FLOW_BTS_FILENAME", "FLOW_CSV_FILENAME", "TURBINE_CONTROL_FILENAME",
		    "AERO_MODEL_FILENAME", "QBLADE_INTERFACE_FILENAME", "DATA_PROCESSING_FILENAME", "BUILD_SHARED_LIBS",
		    "BUILD_AERO_CONTROL_EXECUTABLE", "BUILD_XFE_SCADA_INTERFACE", "INTEGRATE_CUSTOMER_MODELS",
		    "RUN_SINGLE_MODEL_ONLY", "CUSTOMER_NAME", "GIT_TAG_TO_USE"
		]
		# Get current config values from the file (reloaded every time this is called)
		config_values = self.parse_config_file()
		self.config_vars = {}
		row = 0
		for var in editable_vars:
			tk.Label(self.config_window, text=f"{var}:").grid(row=row, column=0, padx=5, pady=5, sticky="w")
			# Load the value from the file; if not found, leave it blank.
			var_str = tk.StringVar(value=config_values.get(var, ""))
			self.config_vars[var] = var_str
			tk.Entry(self.config_window, textvariable=var_str, width=50).grid(row=row, column=1, padx=5, pady=5)
			row += 1
		save_btn = tk.Button(
		    self.config_window, text="Save Config", command=lambda: self.save_config(self.config_window))
		save_btn.grid(row=row, column=0, columnspan=2, pady=10)

	def save_config(self, window):
		# Gather the new config values—if a field is empty, preserve the current value from the file.
		current_values = self.parse_config_file()
		new_values = {}
		for key, var in self.config_vars.items():
			value = var.get().strip()
			if not value:
				new_values[key] = current_values.get(key, "")
			else:
				new_values[key] = value
		try:
			self.update_config_file(new_values)
			messagebox.showinfo("Success", "Config updated successfully!")
			window.destroy()
		except Exception as e:
			messagebox.showerror("Error", f"Failed to update config file: {e}")

if __name__ == "__main__":
	create_sample_csv()
	root = tk.Tk()
	app = CSVEditorApp(root)
	root.mainloop()
