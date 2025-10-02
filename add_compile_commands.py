import json
import os

# Load existing compile_commands.json
with open('build/compile_commands.json', 'r') as f:
    compile_commands = json.load(f)

# Common include paths for the project
common_includes = """
-IC:/Users/jacks/chess_project_v0_espidf/build/config
-IC:/Users/jacks/chess_project_v0_espidf/main
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/newlib/platform_include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/config/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/config/include/freertos
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/config/xtensa/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/FreeRTOS-Kernel/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/FreeRTOS-Kernel/portable/xtensa/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/freertos/esp_additions/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/include/soc
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/include/soc/esp32
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/dma/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/ldo/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/debug_probe/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/mspi_timing_tuning/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/mspi_timing_tuning/tuning_scheme_impl/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/power_supply/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/port/esp32/.
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_hw_support/port/esp32/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/heap/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/heap/tlsf
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/log/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/soc/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/soc/esp32
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/soc/esp32/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/soc/esp32/register
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/hal/platform_port/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/hal/esp32/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/hal/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_rom/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_rom/esp32/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_rom/esp32/include/esp32
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_rom/esp32
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_common/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_system/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_system/port/soc
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/esp_system/port/include/private
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/xtensa/esp32/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/xtensa/include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/xtensa/deprecated_include
-IC:/Users/jacks/esp/v5.5.1/esp-idf/components/pthread/include
""".strip().replace('\n', ' ')

# Common C flags
c_flags = """
-DESP_PLATFORM -DIDF_VER=\"v5.5.1\" -DSOC_MMU_PAGE_SIZE=CONFIG_MMU_PAGE_SIZE -DSOC_XTAL_FREQ_MHZ=CONFIG_XTAL_FREQ
-D_GLIBCXX_HAVE_POSIX_SEMAPHORE -D_GLIBCXX_USE_POSIX_SEMAPHORE -D_GNU_SOURCE -D_POSIX_READER_WRITER_LOCKS
-mlongcalls -Wno-frame-address -fno-builtin-memcpy -fno-builtin-memset -fno-builtin-bzero -fno-builtin-stpcpy -fno-builtin-strncpy
-fdiagnostics-color=always -ffunction-sections -fdata-sections -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-variable
-Wno-error=unused-but-set-variable -Wno-error=deprecated-declarations -Wextra -Wno-error=extra -Wno-unused-parameter -Wno-sign-compare
-Wno-enum-conversion -gdwarf-4 -ggdb -Og -fno-shrink-wrap -fmacro-prefix-map=C:/Users/jacks/chess_project_v0_espidf=.
-fmacro-prefix-map=C:/Users/jacks/esp/v5.5.1/esp-idf=/IDF -fstrict-volatile-bitfields -fno-jump-tables -fno-tree-switch-conversion
""".strip().replace('\n', ' ')

# C++ specific flags
cpp_flags = c_flags + " -std=gnu++2b -fno-exceptions -fno-rtti -fuse-cxa-atexit"
c_only_flags = c_flags + " -std=gnu17 -Wno-old-style-declaration"

# C++ files to add
cpp_files = [
    "BishopPiece.cc",
    "ChessBoard.cc",
    "ChessPiece.cc",
    "KingPiece.cc",
    "KnightPiece.cc",
    "main.cc",
    "main_game.cc",
    "PawnPiece.cc",
    "QueenPiece.cc",
    "RookPiece.cc"
]

# C files to add
c_files = [
    "hello_world_main.c"
]

# Add C++ file entries
for cpp_file in cpp_files:
    entry = {
        "directory": "C:/Users/jacks/chess_project_v0_espidf/build",
        "command": f"C:\\\\Users\\\\jacks\\\\.espressif\\\\tools\\\\xtensa-esp-elf\\\\esp-14.2.0_20241119\\\\xtensa-esp-elf\\\\bin\\\\xtensa-esp32-elf-g++.exe {cpp_flags} {common_includes} -o esp-idf\\\\main\\\\CMakeFiles\\\\__idf_main.dir\\\\{cpp_file}.obj -c C:\\\\Users\\\\jacks\\\\chess_project_v0_espidf\\\\main\\\\{cpp_file}",
        "file": f"C:\\\\Users\\\\jacks\\\\chess_project_v0_espidf\\\\main\\\\{cpp_file}",
        "output": f"esp-idf\\\\main\\\\CMakeFiles\\\\__idf_main.dir\\\\{cpp_file}.obj"
    }
    compile_commands.append(entry)

# Add C file entries
for c_file in c_files:
    entry = {
        "directory": "C:/Users/jacks/chess_project_v0_espidf/build",
        "command": f"C:\\\\Users\\\\jacks\\\\.espressif\\\\tools\\\\xtensa-esp-elf\\\\esp-14.2.0_20241119\\\\xtensa-esp-elf\\\\bin\\\\xtensa-esp32-elf-gcc.exe {c_only_flags} {common_includes} -o esp-idf\\\\main\\\\CMakeFiles\\\\__idf_main.dir\\\\{c_file}.obj -c C:\\\\Users\\\\jacks\\\\chess_project_v0_espidf\\\\main\\\\{c_file}",
        "file": f"C:\\\\Users\\\\jacks\\\\chess_project_v0_espidf\\\\main\\\\{c_file}",
        "output": f"esp-idf\\\\main\\\\CMakeFiles\\\\__idf_main.dir\\\\{c_file}.obj"
    }
    compile_commands.append(entry)

# Save updated compile_commands.json
with open('build/compile_commands.json', 'w') as f:
    json.dump(compile_commands, f, indent=2)

print(f"Added {len(cpp_files)} C++ files and {len(c_files)} C files to compile_commands.json")
print("Total entries now:", len(compile_commands))