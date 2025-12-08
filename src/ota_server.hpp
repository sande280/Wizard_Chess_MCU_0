#pragma once
#include <stdarg.h>

// Start the server (OTA + Log)
void start_ota_server();

// Send custom text to the web logger (for uart_printf)
void send_to_web_log(const char* fmt, va_list args);