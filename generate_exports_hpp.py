with open("krnl_list.txt", "r") as f:
    lines = f.readlines()

ordinals = {}
for line in lines:
    parts = line.strip().split(":")
    if len(parts) == 2:
        ordinals[int(parts[0])] = parts[1].strip()

with open("xbe-inspect/src/kernel/xbox_krnl_exports.hpp", "w") as f:
    f.write("#pragma once\n\n")
    f.write("namespace xbe {\n")
    f.write("namespace kernel {\n\n")
    f.write("static const char* xbox_krnl_names[400] = {\n")
    for i in range(400):
        name = ordinals.get(i, "Unknown")
        f.write(f'    /* {i:3} */ "{name}",\n')
    f.write("};\n\n")
    f.write("} // namespace kernel\n")
    f.write("} // namespace xbe\n")
