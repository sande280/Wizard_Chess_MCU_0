cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Exclude unused esp_lcd component to avoid GCC compiler bug
set(EXCLUDE_COMPONENTS "esp_lcd")

project(Wizard_Chess_MCU_0)
