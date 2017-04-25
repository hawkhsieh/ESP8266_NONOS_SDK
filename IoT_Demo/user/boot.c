#include "boot.h"
#include "mem.h"
#include "osapi.h"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define errf( fmt, args...) \
    do {\
            os_printf( "[ERR] (%s:%d): " fmt, __FILENAME__ , __LINE__ ,##args);\
    } while(0)


#define infof( fmt, args...) \
    do {\
            os_printf( "(%s:%d): " fmt, __FILENAME__ , __LINE__ ,##args);\
    } while(0)


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
        //os_infof("No ram!\r\n");
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


int ICACHE_FLASH_ATTR rboot_get_slot( int current_rom ,FIRMWARE fw ) {
    int slot;
    if (current_rom > 1 ){
        slot = fw+2;
    }else{
        slot = fw;
    }
    return slot;
}



#define RBOOT_RTC_MAGIC 0x2334ae68
#define RBOOT_RTC_READ 1
#define RBOOT_RTC_WRITE 0
#define RBOOT_RTC_ADDR 64

uint32 system_rtc_mem(int32 addr, void *buff, int32 length, uint32 mode) {

    int32 blocks;

    // validate reading a user block
    if (addr < 64) return 0;
    if (buff == 0) return 0;
    // validate 4 byte aligned
    if (((uint32)buff & 0x3) != 0) return 0;
    // validate length is multiple of 4
    if ((length & 0x3) != 0) return 0;

    // check valid length from specified starting point
    if (length > (0x300 - (addr * 4))) return 0;

    // copy the data
    for (blocks = (length >> 2) - 1; blocks >= 0; blocks--) {
        volatile uint32 *ram = ((uint32*)buff) + blocks;
        volatile uint32 *rtc = ((uint32*)0x60001100) + addr + blocks;
        if (mode == RBOOT_RTC_WRITE) {
            *rtc = *ram;
        } else {
            *ram = *rtc;
        }
    }

    return 1;
}


typedef struct {
    uint32 magic;           ///< Magic, identifies rBoot RTC data - should be RBOOT_RTC_MAGIC
    uint8 next_mode;        ///< The next boot mode, defaults to MODE_STANDARD - can be set to MODE_TEMP_ROM
    uint8 last_mode;        ///< The last (this) boot mode - can be MODE_STANDARD, MODE_GPIO_ROM or MODE_TEMP_ROM
    uint8 last_rom;         ///< The last (this) boot rom number
    uint8 temp_rom;         ///< The next boot rom number when next_mode set to MODE_TEMP_ROM
    uint8 chksum;           ///< Checksum of this structure this will be updated for you passed to the API
    uint8 isRma;
    uint8 isBoot;           ///If the application can run,fill this to 'Y' or leave it null.
    uint8 padding[1];
} rboot_rtc_data;

#define CHKSUM_INIT 0xef

uint8 ICACHE_FLASH_ATTR calc_chksum(uint8 *start, uint8 *end) {
    uint8 chksum = CHKSUM_INIT;
    while(start < end) {
        chksum ^= *start;
        start++;
    }
    return chksum;
}

bool ICACHE_FLASH_ATTR rboot_get_rtc_data(rboot_rtc_data *rtc) {
    if (system_rtc_mem_read(RBOOT_RTC_ADDR, rtc, sizeof(rboot_rtc_data))) {
        if (rtc->chksum == calc_chksum((uint8*)rtc, (uint8*)&rtc->chksum)){
            return true;
        }else{
            errf("get rtc data filed\n");
        }
    }
    return false;
}

bool ICACHE_FLASH_ATTR rboot_set_rtc_data(rboot_rtc_data *rtc) {
    // calculate checksum
    rtc->chksum = calc_chksum((uint8*)rtc, (uint8*)&rtc->chksum);
    return system_rtc_mem_write(RBOOT_RTC_ADDR, rtc, sizeof(rboot_rtc_data));
}

#define RBOOT_RTC
#ifdef RBOOT_RTC
int ICACHE_FLASH_ATTR rboot_get_rma( char *isRma , int *last_rom ) {

    rboot_rtc_data rtc;
    if (rboot_get_rtc_data(&rtc)) {
        *isRma = rtc.isRma;
        *last_rom = rtc.last_rom;
        return 0;
    }
    return -1;

}

int ICACHE_FLASH_ATTR rboot_set_rma( char isRma ) {

    rboot_rtc_data rtc;
    if (!rboot_get_rtc_data(&rtc)) {
        rtc.isRma=isRma;
        rboot_set_rtc_data(&rtc);
        return 0;
    }
    return -1;

}
// set current boot firmware
bool ICACHE_FLASH_ATTR rboot_set_current_fw( FIRMWARE fw ) {
    rboot_rtc_data conf;
    if  (rboot_get_rtc_data(&conf) == false ){
        errf("read rtc failed,conf.last_rom=0\n");
        conf.last_rom=0;
    }
    int next_rom=rboot_get_slot( conf.last_rom , fw );
    infof("*** RTC2 Runing at rom[%d] next is rom[%d] ***\n",conf.last_rom,next_rom);
    conf.last_rom = next_rom;

    if (!rboot_set_rtc_data(&conf))
    {
        errf("write rtc failed\n");
    }

    rboot_rtc_data conf_chk;
    rboot_get_rtc_data(&conf_chk);

    if ( conf.last_rom == conf_chk.last_rom)
        return true;
    else{
        errf("conf.last_rom=%d,conf_chk.last_rom=%d\n",conf.last_rom,conf_chk.last_rom);
        return false;
    }
}

// set current boot rom
bool ICACHE_FLASH_ATTR rboot_set_current_rom(uint8 rom) {
    rboot_rtc_data conf;
    if  (rboot_get_rtc_data(&conf) == false ){
        errf("read rtc failed,conf.last_rom=0\n");
        conf.last_rom=0;
    }else{
        conf.last_rom = rom;
    }
    return rboot_set_rtc_data(&conf);
}
#else

int ICACHE_FLASH_ATTR rboot_get_rma( char *isRma , int *last_rom ) {

    rboot_config conf;
    conf = rboot_get_config();
    *isRma = conf.isRma;
    *last_rom=conf.current_rom;
    return 0;

}

int ICACHE_FLASH_ATTR rboot_set_rma( char isRma ) {

    rboot_config conf;
    conf = rboot_get_config();
    conf.isRma = isRma;
    return rboot_set_config(&conf);

}
// set current boot firmware
bool ICACHE_FLASH_ATTR rboot_set_current_fw( FIRMWARE fw ) {
    rboot_config conf;
    conf = rboot_get_config();
    if (conf.current_rom>=conf.count){
        errf("invalid rom set to 0\n");
        conf.current_rom=0;
    }
    int next_rom=rboot_get_slot( conf.current_rom , fw );
    infof("*** Runing at rom[%d] next is rom[%d] ***\n",conf.current_rom,next_rom);
    conf.current_rom = next_rom;

    return rboot_set_config(&conf);
}

// set current boot rom
bool ICACHE_FLASH_ATTR rboot_set_current_rom(uint8 rom) {
    rboot_config conf;
    conf = rboot_get_config();
    if (rom >= conf.count) return false;
    conf.current_rom = rom;
    return rboot_set_config(&conf);
}

#endif
