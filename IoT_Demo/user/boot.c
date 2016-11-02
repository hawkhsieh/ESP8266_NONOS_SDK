#include "boot.h"
#include "mem.h"
#include "osapi.h"

// get the rboot config
rboot_config ICACHE_FLASH_ATTR rboot_get_config(void) {
    rboot_config conf;
    spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)&conf, sizeof(rboot_config));
    return conf;
}

// write the rboot config
// preserves the contents of the rest of the sector,
// so the rest of the sector can be used to store user data
// updates checksum automatically (if enabled)
bool ICACHE_FLASH_ATTR rboot_set_config(rboot_config *conf) {
    uint8 *buffer;
    buffer = (uint8*)os_malloc(SECTOR_SIZE);
    if (!buffer) {
        //os_printf("No ram!\r\n");
        return false;
    }

    spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)((void*)buffer), SECTOR_SIZE);
    os_memcpy(buffer, conf, sizeof(rboot_config));
    spi_flash_erase_sector(BOOT_CONFIG_SECTOR);
    //spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)((void*)buffer), SECTOR_SIZE);
    spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*)((void*)buffer), SECTOR_SIZE);
    os_free(buffer);
    return true;
}

// get current boot rom
uint8 ICACHE_FLASH_ATTR rboot_get_current_rom(void) {
    rboot_config conf;
    conf = rboot_get_config();
    return conf.current_rom;
}

// set current boot rom
bool ICACHE_FLASH_ATTR rboot_set_current_rom(uint8 rom) {
    rboot_config conf;
    conf = rboot_get_config();
    if (rom >= conf.count) return false;
    conf.current_rom = rom;
    return rboot_set_config(&conf);
}
