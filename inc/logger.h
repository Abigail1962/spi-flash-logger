#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

#define LOG_RECORD_SIZE     14  /* Struct size with padding and CRC alignment */
#define SECTOR_SIZE        4096
#define FLASH_LOG_START    0x00000000
#define FLASH_LOG_SECTORS  4    /* Circular logging area of 16KB */
#define FLASH_LOG_END      (FLASH_LOG_START + (FLASH_LOG_SECTORS * SECTOR_SIZE))

#define EVENT_BOOT          0x01
#define EVENT_FAULT         0x02
#define EVENT_SHUTDOWN      0x03

#pragma pack(push, 1) // Force tight packing
typedef struct {
    uint32_t timestamp;     /* Monotonic record sequence, not elapsed seconds */
    uint8_t event_type;     /* Event ID */
    uint8_t dtc[3];         /* 3-byte DTC Code */
    uint16_t voltage;       /* Voltage in 0.1V units */
    uint8_t temperature;    /* Temperature in C (offset 40) */
    uint8_t padding;        /* Padding byte to align CRC */
    uint16_t crc;           /* CRC-16 checksum of bytes 0-11 */
} LogRecord_t;
#pragma pack(pop)

/* Logger APIs */
int Logger_Init(void);
int Logger_WriteRecord(uint8_t event_type, const uint8_t *dtc, uint16_t voltage, uint8_t temperature);
void Logger_PrintAllRecords(void);
uint16_t Compute_CRC16(const uint8_t *data, uint32_t length);

#endif /* LOGGER_H */
