import numpy as np

# Parameters
size = 4096  # LUT size
q_format = 32  # Q32 format with 32 fractional bits

# Generate the Hanning window in floating point
hanning_window = np.hanning(size)

# Convert to Q32 format
hanning_q32 = np.round(hanning_window * (2 ** q_format)).astype(np.uint32)

# Only save half of the array 
hanning_q32 = hanning_q32[:len(hanning_q32)//2]

# Uncomment to save as CSV for analysis
#np.savetxt("hanning_window_q32.csv", hanning_q32, fmt="%u", delimiter=",")

#save as dat file 
with open("hanning_window_q32.dat", "wb") as file:
    file.write(bytearray(hanning_q32))

print("Hanning window LUT saved to 'hanning_window_q32.dat' in Q32 format.")
