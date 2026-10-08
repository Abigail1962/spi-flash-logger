#include "logger.h"
#include "w25qxx.h"
#include <stdio.h>
#include <string.h>

/* Host demonstration: fixed 16-byte slots avoid page/sector straddling.
 * Wire payload remains 14 bytes. Sequence counter is NOT elapsed seconds.
 * CRC rejects most torn records; this is not an atomic/power-safe guarantee. */
#define SLOT_SIZE 16U
static uint32_t g_write_ptr;
static uint32_t g_sequence;

uint16_t Compute_CRC16(const uint8_t *data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}
static uint32_t next_slot(uint32_t addr) {
    addr += SLOT_SIZE;
    return addr >= FLASH_LOG_END ? FLASH_LOG_START : addr;
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int erased(const uint8_t *p) {
    for (unsigned i = 0; i < LOG_RECORD_SIZE; ++i) if (p[i] != 0xFF) return 0;
    return 1;
}
static int valid(const uint8_t *p) {
    uint16_t stored = (uint16_t)p[12] | ((uint16_t)p[13] << 8);
    return !erased(p) && get32(p) != UINT32_MAX &&
           Compute_CRC16(p, 12) == stored;
}
int Logger_Init(void) {
    if (W25Q_Init() != 0) return -1;
    uint8_t data[LOG_RECORD_SIZE];
    uint32_t newest_addr = FLASH_LOG_START, newest_seq = 0;
    int found = 0;
    for (uint32_t addr = FLASH_LOG_START; addr < FLASH_LOG_END; addr += SLOT_SIZE) {
        W25Q_ReadData(addr, data, sizeof(data));
        if (valid(data) && (!found || get32(data) > newest_seq)) {
            newest_addr = addr; newest_seq = get32(data); found = 1;
        }
    }
    g_sequence = found ? newest_seq + 1 : 0;
    g_write_ptr = found ? next_slot(newest_addr) : FLASH_LOG_START;
    /* Never attempt to overwrite programmed/torn bytes without erase.
     * Skip damaged slots inside a sector; reclaim at a sector boundary. */
    for (unsigned i = 0; i < (FLASH_LOG_END - FLASH_LOG_START) / SLOT_SIZE; ++i) {
        if (g_write_ptr % SECTOR_SIZE == 0) break;
        W25Q_ReadData(g_write_ptr, data, sizeof(data));
        if (erased(data)) break;
        g_write_ptr = next_slot(g_write_ptr);
    }
    printf("[Host logger] Next slot: 0x%08X, sequence: %u\n", g_write_ptr, g_sequence);
    return 0;
}
int Logger_WriteRecord(uint8_t event_type, const uint8_t *dtc, uint16_t voltage, uint8_t temperature) {
    if (!dtc || g_sequence == UINT32_MAX) return -1;
    uint8_t data[LOG_RECORD_SIZE] = {0}, readback[LOG_RECORD_SIZE];
    for (unsigned i = 0; i < 4; ++i) data[i] = (uint8_t)(g_sequence >> (8 * i));
    data[4] = event_type;
    memcpy(data + 5, dtc, 3);
    data[8] = (uint8_t)voltage; data[9] = (uint8_t)(voltage >> 8);
    data[10] = temperature;
    uint16_t crc = Compute_CRC16(data, 12);
    data[12] = (uint8_t)crc; data[13] = (uint8_t)(crc >> 8);
    if (g_write_ptr % SECTOR_SIZE == 0 && W25Q_SectorErase(g_write_ptr) != 0) return -1;
    if (W25Q_PageProgram(g_write_ptr, data, sizeof(data)) != 0) return -1;
    W25Q_ReadData(g_write_ptr, readback, sizeof(readback));
    g_write_ptr = next_slot(g_write_ptr);
    if (memcmp(data, readback, sizeof(data))) return -1;
    ++g_sequence;
    return 0;
}
void Logger_PrintAllRecords(void) {
    uint8_t data[LOG_RECORD_SIZE];
    unsigned count = 0;
    puts("Address | Sequence | Event | Voltage | Temperature (physical slot order)");
    for (uint32_t addr = FLASH_LOG_START; addr < FLASH_LOG_END; addr += SLOT_SIZE) {
        W25Q_ReadData(addr, data, sizeof(data));
        if (!valid(data)) continue;
        unsigned mv = (unsigned)data[8] | ((unsigned)data[9] << 8);
        printf("0x%08X | %u | %u | %.1fV | %dC\n", addr, get32(data), data[4], mv / 10.0, (int)data[10] - 40);
        ++count;
    }
    printf("Valid host records: %u\n", count);
}
