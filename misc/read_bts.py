"""
TurbSim BTS (Binary Time Series) File Reader

This module reads and processes TurbSim BTS files containing turbulent wind field data.
Supports both int16 and float32 data formats.
"""

import struct
import numpy as np
from pathlib import Path
from typing import Optional, Tuple

class BTSData:
	"""
    Data structure for storing TurbSim BTS dataset and metadata.
    
    Attributes:
        nz (int): Number of vertical grid points
        ny (int): Number of horizontal grid points
        ntwr (int): Number of tower measurement points
        nt (int): Number of time steps
        dz (float): Vertical grid spacing (m)
        dy (float): Horizontal grid spacing (m)
        dt (float): Time step size (s)
        mffws (float): Mean wind speed (m/s)
        zHub (float): Hub height (m)
        z1 (float): Bottom grid height (m)
        Vslope (np.ndarray): Slope values for U, V, W components [3]
        Voffset (np.ndarray): Offset values for U, V, W components [3]
        description (str): Description string from file
        velocity (np.ndarray): Velocity data array of shape (nt, 3, ny, nz)
        twrVelocity (np.ndarray): Tower velocity array of shape (nt, 3, ntwr)
        y (np.ndarray): Horizontal coordinate array
        z (np.ndarray): Vertical coordinate array
        zTwr (np.ndarray): Tower height coordinate array
    """

	def __init__(self):
		self.nz = 0
		self.ny = 0
		self.ntwr = 0
		self.nt = 0
		self.dz = 0.0
		self.dy = 0.0
		self.dt = 0.0
		self.mffws = 0.0
		self.zHub = 0.0
		self.z1 = 0.0
		self.Vslope = np.zeros(3)
		self.Voffset = np.zeros(3)
		self.description = ""
		self.velocity = None
		self.twrVelocity = None
		self.y = None
		self.z = None
		self.zTwr = None

def read_bts_file(file_path: str, file_format: str = "int16", verbose: bool = True) -> BTSData:
	"""
    Read a TurbSim BTS binary file.
    
    Args:
        file_path: Path to the BTS file
        file_format: Data format - either "int16" or "float32"
        verbose: If True, print progress messages
        
    Returns:
        BTSData object containing all wind field data
        
    Raises:
        FileNotFoundError: If the file doesn't exist
        ValueError: If file format is invalid or file is corrupted
    """

	file_path = Path(file_path)
	if not file_path.exists():
		raise FileNotFoundError(f"Could not open the wind file: {file_path}")

	if verbose:
		print(f"Reading BTS file: {file_path}")

	data = BTSData()

	# Determine data type
	if file_format == "float32":
		data_dtype = np.float32
		data_size = 4
		if verbose:
			print("Using float32 format for velocity data")
	else:
		data_dtype = np.int16
		data_size = 2
		if verbose:
			print("Using int16 format for velocity data")

	with open(file_path, 'rb') as f:
		# Read TurbSim format identifier
		format_id = struct.unpack('h', f.read(2))[0]
		if verbose:
			print(f"TurbSim format identifier: {format_id}")

		# Read basic grid dimensions
		data.nz = struct.unpack('i', f.read(4))[0]
		data.ny = struct.unpack('i', f.read(4))[0]
		data.ntwr = struct.unpack('i', f.read(4))[0]
		data.nt = struct.unpack('i', f.read(4))[0]

		if verbose:
			print(f"Grid points (vertical): {data.nz}")
			print(f"Grid points (horizontal): {data.ny}")
			print(f"Tower points: {data.ntwr}")
			print(f"Number of time steps: {data.nt}")

		# Read grid spacing and parameters (stored as float32 in file)
		data.dz = struct.unpack('f', f.read(4))[0]
		data.dy = struct.unpack('f', f.read(4))[0]
		data.dt = struct.unpack('f', f.read(4))[0]
		data.mffws = struct.unpack('f', f.read(4))[0]
		data.zHub = struct.unpack('f', f.read(4))[0]
		data.z1 = struct.unpack('f', f.read(4))[0]

		if verbose:
			print(f"Grid spacing (vertical): {data.dz:.4f} m")
			print(f"Grid spacing (horizontal): {data.dy:.4f} m")
			print(f"Time step size: {data.dt:.4f} s")
			print(f"Mean wind speed: {data.mffws:.4f} m/s")
			print(f"Hub height: {data.zHub:.4f} m")
			print(f"Bottom grid height: {data.z1:.4f} m")

		# Read slope and offset for velocity components
		for i in range(3):
			data.Vslope[i] = struct.unpack('f', f.read(4))[0]
			data.Voffset[i] = struct.unpack('f', f.read(4))[0]
			if verbose:
				print(f"Vslope[{i}]: {data.Vslope[i]:.4f}, Voffset[{i}]: {data.Voffset[i]:.4f}")

		# Read description string
		nchar = struct.unpack('i', f.read(4))[0]
		nchar = min(nchar, 200)
		data.description = f.read(nchar).decode('utf-8', errors='ignore')
		if verbose:
			print(f"Description: {data.description}")

		# Allocate arrays for velocity data
		nffc = 3  # Number of velocity components (U, V, W)
		n_pts = data.ny * data.nz
		nv = nffc * n_pts
		nv_twr = nffc * data.ntwr

		# Initialize velocity arrays
		data.velocity = np.zeros((data.nt, nffc, data.ny, data.nz), dtype=np.float64)
		data.twrVelocity = np.zeros((data.nt, nffc, data.ntwr), dtype=np.float64)

		if verbose:
			print(f"Reading {data.nt} time steps...")

		# Read time series data
		for it in range(data.nt):
			# Read grid velocity data
			v_raw = np.fromfile(f, dtype=data_dtype, count=nv)

			if len(v_raw) < nv:
				raise ValueError(f"Unexpected end of file at time step {it}")

			# Reshape and scale velocity data
			v_raw = v_raw.reshape((nffc, data.ny, data.nz))

			for k in range(nffc):
				if file_format == "float32":
					data.velocity[it, k, :, :] = v_raw[k, :, :]
				else:
					# Apply scaling for int16 format
					data.velocity[it, k, :, :] = (v_raw[k, :, :] - data.Voffset[k]) / data.Vslope[k]

			# Read tower velocity data if present
			if nv_twr > 0:
				v_twr_raw = np.fromfile(f, dtype=data_dtype, count=nv_twr)

				if len(v_twr_raw) < nv_twr:
					raise ValueError(f"Unexpected end of file at tower data, time step {it}")

				v_twr_raw = v_twr_raw.reshape((nffc, data.ntwr))

				for k in range(nffc):
					if file_format == "float32":
						data.twrVelocity[it, k, :] = v_twr_raw[k, :]
					else:
						data.twrVelocity[it, k, :] = (v_twr_raw[k, :] - data.Voffset[k]) / data.Vslope[k]

	# Calculate coordinate arrays
	data.y = np.array([iy * data.dy - (data.dy * (data.ny - 1)) / 2.0 for iy in range(data.ny)])
	data.z = np.array([iz * data.dz + data.z1 for iz in range(data.nz)])
	data.zTwr = np.array([data.z1 - iz * data.dz for iz in range(data.ntwr)])

	if verbose:
		print(f"Finished reading BTS file: {file_path}")
		print(f"Total simulation time: {data.nt * data.dt:.2f} s")

	return data

def find_bts_y_z_position(data: BTSData,
                          horizontal_y_position: float,
                          vertical_z_position: Optional[float] = None) -> Tuple[int, int]:
	"""
    Find the grid indices (iy, iz) closest to the given position.
    
    Args:
        data: BTSData object
        horizontal_y_position: Horizontal position (m)
        vertical_z_position: Vertical position (m). If None, uses hub height.
        
    Returns:
        Tuple of (iy, iz) indices
    """

	# Use hub height if no vertical position given
	if vertical_z_position is None:
		vertical_z_position = data.zHub

	# Calculate y-axis origin (center of grid)
	y_origin = -(data.dy * (data.ny - 1)) / 2.0
	y_min = y_origin
	y_max = y_origin + (data.ny - 1) * data.dy

	# Clamp horizontal position to grid bounds
	if horizontal_y_position < y_min:
		print(f"Warning: horizontal_y_position({horizontal_y_position:.2f}) < y_min({y_min:.2f}), clamping")
		horizontal_y_position = y_min
	elif horizontal_y_position > y_max:
		print(f"Warning: horizontal_y_position({horizontal_y_position:.2f}) > y_max({y_max:.2f}), clamping")
		horizontal_y_position = y_max

	# Find closest y index
	iy = int(round((horizontal_y_position - y_origin) / data.dy))
	iy = np.clip(iy, 0, data.ny - 1)

	# Calculate z bounds
	z_min = data.z1
	z_max = data.z1 + (data.nz - 1) * data.dz

	# Clamp vertical position to grid bounds
	if vertical_z_position < z_min:
		print(f"Warning: vertical_z_position({vertical_z_position:.2f}) < z_min({z_min:.2f}), clamping")
		vertical_z_position = z_min
	elif vertical_z_position > z_max:
		print(f"Warning: vertical_z_position({vertical_z_position:.2f}) > z_max({z_max:.2f}), clamping")
		vertical_z_position = z_max

	# Find closest z index
	iz = int(round((vertical_z_position - data.z1) / data.dz))
	iz = np.clip(iz, 0, data.nz - 1)

	return iy, iz

def get_velocity_magnitude_timeseries(
        data: BTSData, horizontal_y_position: float = 0.0, vertical_z_position: Optional[float] = None) -> np.ndarray:
	"""
    Extract horizontal velocity magnitude time series at a given position.
    
    Args:
        data: BTSData object
        horizontal_y_position: Horizontal position (m), default 0.0 (centerline)
        vertical_z_position: Vertical position (m). If None, uses hub height.
        
    Returns:
        Array of velocity magnitudes for each time step
    """

	iy, iz = find_bts_y_z_position(data, horizontal_y_position, vertical_z_position)

	# Extract U and V components
	u = data.velocity[:, 0, iy, iz]  # X-component
	v = data.velocity[:, 1, iy, iz]  # Y-component

	# Calculate magnitude (ignoring W component)
	u_mag = np.sqrt(u**2 + v**2)

	return u_mag

def interpolate_velocity(vel_data: np.ndarray, dt: float, current_time: float) -> float:
	"""
    Linearly interpolate velocity magnitude at a given time.
    
    Args:
        vel_data: Array of velocity values at regular time intervals
        dt: Time step between velocity values
        current_time: Time at which to interpolate
        
    Returns:
        Interpolated velocity value
    """

	num_time_steps = len(vel_data)

	# Calculate indices
	lower_idx = int(np.floor(current_time / dt))
	upper_idx = lower_idx + 1

	# Clamp to bounds
	if lower_idx < 0:
		lower_idx = 0
		upper_idx = 1
	elif upper_idx >= num_time_steps:
		upper_idx = num_time_steps - 1
		lower_idx = upper_idx - 1

	# Get time and velocity values
	t_lower = lower_idx * dt
	t_upper = upper_idx * dt
	umag_lower = vel_data[lower_idx]
	umag_upper = vel_data[upper_idx]

	# Linear interpolation
	if t_upper == t_lower:
		return umag_lower

	weight = (current_time - t_lower) / (t_upper - t_lower)
	interpolated_umag = umag_lower + weight * (umag_upper - umag_lower)

	return interpolated_umag

def precompute_flow_interpolation(
        bts_data: BTSData,
        simulation_dt: float,
        y_position: float = 0.0,
        z_position: Optional[float] = None) -> Tuple[np.ndarray, np.ndarray]:
	"""
    Precompute interpolated flow speeds for a full simulation.
    
    This is the Python equivalent of the bts_fixed_interp_flow_gen use case.
    
    Args:
        bts_data: BTSData object with wind field data
        simulation_dt: Simulation time step (s)
        y_position: Horizontal position for extraction (m)
        z_position: Vertical position for extraction (m), None for hub height
        
    Returns:
        Tuple of (time_array, flow_speed_array)
    """

	# Extract velocity magnitude time series
	vel_data = get_velocity_magnitude_timeseries(bts_data, y_position, z_position)

	# Calculate total available time
	total_time = bts_data.nt * bts_data.dt

	# Generate simulation time array
	num_sim_steps = int(total_time / simulation_dt) + 1
	time_array = np.arange(num_sim_steps) * simulation_dt

	# Precompute interpolated values
	flow_speed_array = np.zeros(num_sim_steps)

	for i, t in enumerate(time_array):
		flow_speed_array[i] = interpolate_velocity(vel_data, bts_data.dt, t)

	return time_array, flow_speed_array

# Example usage
if __name__ == "__main__":
	# Example: Read a BTS file and extract flow data

	# Read BTS file
	bts_file = "path/to/your/file.bts"

	try:
		bts_data = read_bts_file(bts_file, file_format="int16", verbose=True)

		# Extract velocity at hub height on centerline
		print("\n" + "=" * 60)
		print("Extracting velocity magnitude at hub height...")
		vel_mag = get_velocity_magnitude_timeseries(
		    bts_data, horizontal_y_position=0.0, vertical_z_position=None)  # None = hub height

		print(f"Velocity range: {vel_mag.min():.2f} - {vel_mag.max():.2f} m/s")
		print(f"Mean velocity: {vel_mag.mean():.2f} m/s")

		# Precompute for a simulation
		print("\n" + "=" * 60)
		print("Precomputing interpolation for simulation...")
		sim_dt = 0.01  # 10 ms simulation time step
		time_array, flow_speeds = precompute_flow_interpolation(bts_data, sim_dt)

		print(f"Precomputed {len(time_array)} time steps")
		print(f"Simulation duration: {time_array[-1]:.2f} s")

		# Example: Get flow speed at arbitrary time
		query_time = 10.5  # seconds
		if query_time <= time_array[-1]:
			flow_speed = interpolate_velocity(vel_mag, bts_data.dt, query_time)
			print(f"\nFlow speed at t={query_time}s: {flow_speed:.2f} m/s")

	except FileNotFoundError as e:
		print(f"Error: {e}")
		print("Please provide a valid BTS file path in the example.")
	except Exception as e:
		print(f"Error reading BTS file: {e}")
