#ifndef _BOOT_H
#define _BOOT_H

#include "c_types.h"


#define MAX_ROMS 4
#define BOOT_CONFIG_SECTOR 1
#define SECTOR_SIZE 0x1000

typedef struct {
    uint8 magic;           ///< Our magic, identifies rBoot configuration - should be BOOT_CONFIG_MAGIC
    uint8 version;         ///< Version of configuration structure - should be BOOT_CONFIG_VERSION
    uint8 mode;            ///< Boot loader mode (MODE_STANDARD | MODE_GPIO_ROM)
    uint8 current_rom;     ///< Currently selected ROM (will be used for next standard boot)
    uint8 gpio_rom;        ///< ROM to use for GPIO boot (hardware switch) with mode set to MODE_GPIO_ROM
    uint8 count;           ///< Quantity of ROMs available to boot
    uint8 unused[2];       ///< Padding (not used)
    uint32 roms[MAX_ROMS]; ///< Flash addresses of each ROM
} rboot_config;


bool ICACHE_FLASH_ATTR rboot_set_current_rom(uint8 rom);


#endif
