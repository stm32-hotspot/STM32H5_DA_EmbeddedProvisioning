import os
import sys

def binary_to_c_header(input_file):
    base_name = os.path.splitext(os.path.basename(input_file))[0]

    with open(input_file, 'rb') as f:
        data = f.read()

    output_file = f"{base_name}.h"

    with open(output_file, 'w') as f:
        f.write(f"const unsigned char {base_name}[] = {{\n")
        for i, byte in enumerate(data):
            if i % 16 == 0:
                f.write("\n    ")
            f.write(f"0x{byte:02x}, ")
        f.write("\n};")

    print(f"Conversion successful. C header file '{output_file}' created.")

default_input_file = "DA_Config.obk"

if len(sys.argv) < 2:
    binary_to_c_header(default_input_file)
else:
    binary_file_path = sys.argv[1]
    binary_to_c_header(binary_file_path)