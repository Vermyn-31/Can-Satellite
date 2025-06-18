from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import math

# Get the relative path to the text file
fpath = Path.cwd().joinpath('putty.log')

# Initialize the list
data_list = []
co2_values = []
pm_values = []
temp_values = []
humidity_values = []
altitude_values = []
lat_values = []
lon_values = []

# Define required parameters
REQUIRED_PARAMS = ['Local Time', 'Altitude', 'Latitude', 'Longitude', 
                  'GMaps', 'C02 Readings', 'PM Readings',  
                  'Temperature Readings', 'Humidity Reading']  

# Read and parse the file
with open(fpath, 'r') as f:
    current_entry = {}
    
    for line in f:
        line = line.strip()
        
        # Check for header
        if line == '[D.L~N~R]':
            if current_entry:
                gps_status = 'Active' if all(param in current_entry for param in REQUIRED_PARAMS) else 'Inactive'
                
                # Remove GPS_Status from readings if it exists
                current_entry.pop('GPS_Status', None)
                
                data_list.append([
                    "[D.L~N~R]",
                    {'GPS Status': gps_status},
                    current_entry.copy()
                ])
            
            current_entry = {}
            continue
        
        # Check for GPS signal status
        if line == 'No GPS Fixed Signal available':
            current_entry['GPS_Status'] = 'Inactive'
            continue
        
        # Parse parameters
        for prefix in ['Local Time:', 'Altitude:', 'Latitude:', 'Longitude:', 
                      'GMaps:', 'C02 Readings:', 'PM Readings:',  
                      'Temperature Readings:', 'Humidity Reading:']: 
            if line.startswith(prefix):
                try:
                    key = prefix[:-1]  
                    value = line.split(':', 1)[1].strip()
                    
                    if key == 'Local Time':
                        current_entry[key] = {'value': value}
                    elif key == 'GMaps':
                        current_entry[key] = value
                    else:
                        parts = value.split(' ', 1)
                        if len(parts) == 2:
                            current_entry[key] = {'value': parts[0], 'unit': parts[1]}
                        else:
                            current_entry[key] = {'value': parts[0], 'unit': ''}
                except (IndexError, ValueError):
                    pass  
                break

    if current_entry:
        gps_status = 'Active' if all(param in current_entry for param in REQUIRED_PARAMS) else 'Inactive'
        current_entry.pop('GPS_Status', None)
        data_list.append([
            "[D.L~N~R]",
            {'GPS Status': gps_status},
            current_entry.copy()
        ])

# Filter the data_list
data_list = [data for data in data_list if data[0] == "[D.L~N~R]" and data[1]["GPS Status"] == "Active"]

# Extract values for analysis
for entry in data_list:
    co2_values.append(float(entry[2]['C02 Readings']['value']))
    pm_values.append(float(entry[2]['PM Readings']['value'])*100)
    temp_values.append(float(entry[2]['Temperature Readings']['value']))
    humidity_values.append(float(entry[2]['Humidity Reading']['value'])+25)
    altitude_values.append(float(entry[2]['Altitude']['value']) + 9.15)

    # Convert NMEA format to decimal degrees for latitude
    lat_read = entry[2]['Latitude']['value']
    lat_deg = math.floor(float(lat_read)/100)
    lat_min = float(lat_read) - (lat_deg*100)
    lat = lat_deg + (lat_min/60)
    if entry[2]['Latitude']['unit'] == 'S':
        lat = -lat
    lat_values.append(lat)
    
    # Convert NMEA format to decimal degrees for longitude
    lon_read = entry[2]['Longitude']['value']
    lon_deg = math.floor(float(lon_read)/100)
    lon_min = float(lon_read) - (lon_deg*100)
    lon = lon_deg + (lon_min/60)
    if entry[2]['Longitude']['unit'] == 'W':
        lon = -lon
    lon_values.append(lon)

# Calculate mean and standard deviation for each parameter
def calculate_stats(values):
    return np.mean(values), np.std(values)

co2_mean, co2_std = calculate_stats(co2_values)
pm_mean, pm_std = calculate_stats(pm_values)
temp_mean, temp_std = calculate_stats(temp_values)
humidity_mean, humidity_std = calculate_stats(humidity_values)
altitude_mean, altitude_std = calculate_stats(altitude_values)
lat_mean, lat_std = calculate_stats(lat_values)
lon_mean, lon_std = calculate_stats(lon_values)

# Print the statistics
print("Parameter Statistics:")
print(f"CO2: Mean = {co2_mean:.2f} ppm, Std Dev = {co2_std:.2f} ppm")
print(f"PM: Mean = {pm_mean:.2f} µg/m³, Std Dev = {pm_std:.2f} µg/m³")
print(f"Temperature: Mean = {temp_mean:.2f} °C, Std Dev = {temp_std:.2f} °C")
print(f"Humidity: Mean = {humidity_mean:.2f} %, Std Dev = {humidity_std:.2f} %")
print(f"Altitude: Mean = {altitude_mean:.2f} m, Std Dev = {altitude_std:.2f} m")

# Plot the results with mean and std dev lines and shaded areas
plt.figure(figsize=(12, 10))

# CO2 Plot
plt.subplot(4, 1, 1)
plt.plot(co2_values, 'r-', label='CO2')
# Add mean line
plt.axhline(y=co2_mean, color='k', linestyle='--', label='Mean')
# Add ±1 std dev lines
plt.axhline(y=co2_mean + co2_std, color='gray', linestyle=':', label='±1 Std Dev')
plt.axhline(y=co2_mean - co2_std, color='gray', linestyle=':')
# Shade the std dev area
plt.fill_between(range(len(co2_values)), 
                co2_mean - co2_std, 
                co2_mean + co2_std, 
                color='gray', alpha=0.2)
plt.ylabel('CO2 (PPM)')
plt.title(f'CO2 Readings (Typical Range: {co2_mean:.2f} ± {co2_std:.2f} ppm)')
plt.grid(True)
plt.legend()

# PM Plot
plt.subplot(4, 1, 2)
plt.plot(pm_values, 'b-', label='PM')
plt.axhline(y=pm_mean, color='k', linestyle='--')
plt.axhline(y=pm_mean + pm_std, color='gray', linestyle=':')
plt.axhline(y=pm_mean - pm_std, color='gray', linestyle=':')
plt.fill_between(range(len(pm_values)), 
                pm_mean - pm_std, 
                pm_mean + pm_std, 
                color='gray', alpha=0.2)
plt.ylabel('PM Fine Particles (µg/m^3)')
plt.title(f'PM Readings (Typical Range: {pm_mean:.2f} ± {pm_std:.2f} µg/m³)')
plt.grid(True)
plt.legend()

# Temperature Plot
plt.subplot(4, 1, 3)
plt.plot(temp_values, 'g-', label='Temperature')
plt.axhline(y=temp_mean, color='k', linestyle='--')
plt.axhline(y=temp_mean + temp_std, color='gray', linestyle=':')
plt.axhline(y=temp_mean - temp_std, color='gray', linestyle=':')
plt.fill_between(range(len(temp_values)), 
                temp_mean - temp_std, 
                temp_mean + temp_std, 
                color='gray', alpha=0.2)
plt.ylabel('Temperature (°C)')
plt.title(f'Temperature (Typical Range: {temp_mean:.2f} ± {temp_std:.2f} °C)')
plt.grid(True)
plt.legend()

# Humidity Plot
plt.subplot(4, 1, 4)
plt.plot(humidity_values, 'm-', label='Humidity')
plt.axhline(y=humidity_mean, color='k', linestyle='--')
plt.axhline(y=humidity_mean + humidity_std, color='gray', linestyle=':')
plt.axhline(y=humidity_mean - humidity_std, color='gray', linestyle=':')
plt.fill_between(range(len(humidity_values)), 
                humidity_mean - humidity_std, 
                humidity_mean + humidity_std, 
                color='gray', alpha=0.2)
plt.ylabel('Humidity (%)')
plt.title(f'Humidity (Typical Range: {humidity_mean:.2f} ± {humidity_std:.2f} %)')
plt.xlabel(f'Measured parameter for the duration {data_list[0][2]["Local Time"]["value"]} to {data_list[-1][2]["Local Time"]["value"]}')
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.show()

# Plot Altitude in a separate figure with mean and std dev lines
plt.figure(figsize=(10, 5))
plt.plot(altitude_values, 'c-', label='Altitude')
plt.axhline(y=altitude_mean, color='k', linestyle='--', label='Mean')
plt.axhline(y=altitude_mean + altitude_std, color='gray', linestyle=':', label='±1 Std Dev')
plt.axhline(y=altitude_mean - altitude_std, color='gray', linestyle=':')
plt.fill_between(range(len(altitude_values)), 
                altitude_mean - altitude_std, 
                altitude_mean + altitude_std, 
                color='gray', alpha=0.2)
plt.ylabel('Altitude (m)')
plt.title(f'Altitude Readings (Typical Range: {altitude_mean:.2f} ± {altitude_std:.2f} m)')
plt.xlabel(f'Measured parameter for the duration {data_list[0][2]["Local Time"]["value"]} to {data_list[-1][2]["Local Time"]["value"]}')
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.show()

# Create a new figure for GPS coordinates plot
plt.figure(figsize=(12, 6))

# Plot latitude and longitude over time
plt.subplot(2, 1, 1)
plt.plot(lat_values, 'b-', label='Latitude')
plt.axhline(y=lat_mean, color='k', linestyle='--', label='Mean')
plt.axhline(y=lat_mean + lat_std, color='gray', linestyle=':', label='±1 Std Dev')
plt.axhline(y=lat_mean - lat_std, color='gray', linestyle=':')
plt.fill_between(range(len(lat_values)), 
                lat_mean - lat_std, 
                lat_mean + lat_std, 
                color='gray', alpha=0.2)
plt.ylabel('Latitude (°)')
plt.title(f'Latitude Readings (Typical Range: {lat_mean:.6f} ± {lat_std:.6f}°)')
plt.grid(True)
plt.legend()

plt.subplot(2, 1, 2)
plt.plot(lon_values, 'r-', label='Longitude')
plt.axhline(y=lon_mean, color='k', linestyle='--', label='Mean')
plt.axhline(y=lon_mean + lon_std, color='gray', linestyle=':', label='±1 Std Dev')
plt.axhline(y=lon_mean - lon_std, color='gray', linestyle=':')
plt.fill_between(range(len(lon_values)), 
                lon_mean - lon_std, 
                lon_mean + lon_std, 
                color='gray', alpha=0.2)
plt.ylabel('Longitude (°)')
plt.title(f'Longitude Readings (Typical Range: {lon_mean:.6f} ± {lon_std:.6f}°)')
plt.xlabel('Measurement index')
plt.grid(True)
plt.legend()

plt.tight_layout()
plt.show()

# Create a scatter plot of the coordinates
plt.figure(figsize=(10, 8))
plt.scatter(lon_values, lat_values, c=range(len(lon_values)), cmap='viridis')
plt.colorbar(label='Measurement sequence')
plt.xlabel('Longitude (°)')
plt.ylabel('Latitude (°)')
plt.title('GPS Coordinates Plot')
plt.grid(True)

# Plot mean point
plt.scatter([lon_mean], [lat_mean], c='red', s=100, label=f'Mean ({lat_mean:.6f}°, {lon_mean:.6f}°)')

# Plot standard deviation ellipse
from matplotlib.patches import Ellipse
ellipse = Ellipse(xy=(lon_mean, lat_mean),
                 width=2*lon_std,
                 height=2*lat_std,
                 edgecolor='r',
                 fc='None',
                 lw=2,
                 label='±1 Std Dev')
plt.gca().add_patch(ellipse)

plt.legend()
plt.tight_layout()
plt.show()