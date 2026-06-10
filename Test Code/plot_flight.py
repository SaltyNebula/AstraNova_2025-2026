import csv
import matplotlib.pyplot as plt
import argparse
import sys

def main():
    parser = argparse.ArgumentParser(description='Plot FlightLogger Data')
    parser.add_argument('csv_file', nargs='?', default='flightlog.csv', help='Path to flightlog.csv')
    args = parser.parse_args()

    time_s = []
    rel_altitude = []
    dht_temp = []
    imu_temp = []
    
    base_altitude = None

    try:
        with open(args.csv_file, 'r') as f:
            reader = csv.DictReader(f)
            
            for row in reader:
                event = row.get('Event', '').strip()
                try:
                    alt = float(row['AltitudeM'])
                    time_ms = float(row['TimeMs'])
                    dhtt = float(row['DHTTempC'])
                    imut = float(row['IMUTempC'])
                except (ValueError, KeyError):
                    continue # Skip malformed rows
                    
                if event == 'STARTUP':
                    base_altitude = alt
                    print(f"Found STARTUP calibration row. Base altitude set to {base_altitude:.2f} m")
                elif event == 'LOG':
                    if base_altitude is None:
                        # Fallback if STARTUP row is somehow missing
                        base_altitude = alt
                        print(f"Warning: No STARTUP row found. Using first LOG row absolute altitude ({base_altitude:.2f} m) as zero reference.")
                    
                    time_s.append(time_ms / 1000.0)
                    rel_altitude.append((alt - base_altitude) * 3.28084) # Convert to feet
                    dht_temp.append(dhtt)
                    imu_temp.append(imut)
                    
    except FileNotFoundError:
        print(f"Error: Could not find '{args.csv_file}'.")
        print("Please ensure the SD card is plugged in and you provided the correct file path.")
        sys.exit(1)

    if not time_s:
        print("Error: No valid LOG data found in the CSV.")
        sys.exit(1)
        
    # Offset time so the graph starts exactly at 0 seconds
    start_time = time_s[0]
    time_s = [t - start_time for t in time_s]

    # Create the plot
    fig, ax1 = plt.subplots(figsize=(10, 6))

    # Plot Relative Altitude on the primary (left) Y-axis
    color = 'tab:blue'
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('Relative Altitude (feet)', color=color, fontweight='bold')
    ax1.plot(time_s, rel_altitude, color=color, linewidth=2.5, label='Relative Altitude')
    ax1.tick_params(axis='y', labelcolor=color)
    ax1.grid(True, linestyle='--', alpha=0.6)

    # Plot Temperatures on the secondary (right) Y-axis
    ax2 = ax1.twinx()  
    
    color_dht = 'tab:red'
    color_imu = 'tab:orange'
    ax2.set_ylabel('Temperature (°C)', color='black', fontweight='bold')  
    ax2.plot(time_s, dht_temp, color=color_dht, linestyle='--', linewidth=2, label='DHT22 Temp (Ambient)')
    ax2.plot(time_s, imu_temp, color=color_imu, linestyle='-.', linewidth=2, label='IMU Temp (Board)')
    ax2.tick_params(axis='y', labelcolor='black')

    # Formatting and legends
    fig.suptitle('Rocket Flight Data Analysis', fontsize=16, fontweight='bold')
    fig.tight_layout()
    
    # Combine legends from both axes
    lines_1, labels_1 = ax1.get_legend_handles_labels()
    lines_2, labels_2 = ax2.get_legend_handles_labels()
    ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper left')

    print("Generating plot...")
    plt.show()

if __name__ == '__main__':
    main()
