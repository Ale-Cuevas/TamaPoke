#pragma once
// One flat address space on the PC: PROGMEM is a no-op, reads dereference.
#define PROGMEM
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t *>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t *>(addr))
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t *>(addr))
#define pgm_read_float(addr) (*reinterpret_cast<const float *>(addr))
#define pgm_read_ptr(addr) (*reinterpret_cast<const void *const *>(addr))
