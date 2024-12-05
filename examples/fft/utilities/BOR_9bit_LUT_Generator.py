import struct

def reverse_bits(value, bit_length=9):
    """Reverse the bit order of a value with the specified bit length."""
    reversed_value = 0
    for _ in range(bit_length):
        reversed_value = (reversed_value << 1) | (value & 1)
        value >>= 1
    return reversed_value

# Open a binary file to store the values
with open("bit_reversed_values.dat", "wb") as file:
    for i in range(2**9):  # Iterate through all 12-bit numbers (0 to 4095)
        reversed_value = reverse_bits(i, bit_length=9)
        # Pack the reversed value into a 16-bit binary format and write to the file
        file.write(struct.pack("<H", reversed_value))  # "<H" is for little-endian 16-bit

print("Bit-reversed values have been saved to 'bit_reversed_values.dat'")