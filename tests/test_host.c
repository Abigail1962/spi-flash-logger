#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "w25qxx.h"

int main(void) {
    assert(Compute_CRC16((const uint8_t *)"123456789", 9) == 0x29B1);
    assert(W25Q_Init() == 0);
    SPI_SimSetBusy(1);
    assert(W25Q_WaitBusy() == -1);
    SPI_SimSetBusy(0);
    W25Q_SectorErase(0);
    uint8_t payload[64], result[64];
    for (unsigned i = 0; i < sizeof(payload); ++i) payload[i] = (uint8_t)(i + 1);
    W25Q_PageProgram(250, payload, sizeof(payload));
    W25Q_ReadData(250, result, sizeof(result));
    assert(!memcmp(payload, result, sizeof(result))); /* Driver splits pages. */
    uint8_t zero = 0, ff = 0xFF;
    W25Q_PageProgram(100, &zero, 1);
    W25Q_PageProgram(100, &ff, 1);
    W25Q_ReadData(100, result, 1);
    assert(result[0] == 0); /* Programming cannot turn 0 back into 1. */
    for (unsigned sector = 0; sector < 4; ++sector) W25Q_SectorErase(sector * SECTOR_SIZE);
    Logger_Init();
    uint8_t dtc[3] = {1, 2, 3};
    for (unsigned i = 0; i < 300; ++i) assert(Logger_WriteRecord(EVENT_FAULT, dtc, 420, 65) == 0);
    uint8_t record[14];
    W25Q_ReadData(299 * 16, record, sizeof(record));
    assert(Compute_CRC16(record, 12) == ((uint16_t)record[12] | ((uint16_t)record[13] << 8)));
    /* Incomplete program at the next slot; boot scan must skip damaged bytes. */
    uint8_t partial[5] = {0, 0, 0, 0, 1};
    W25Q_PageProgram(300 * 16, partial, sizeof(partial));
    Logger_Init();
    assert(Logger_WriteRecord(EVENT_BOOT, dtc, 421, 66) == 0);
    W25Q_ReadData(301 * 16, record, sizeof(record));
    assert(record[0] == (300 & 255) && record[1] == (300 >> 8));
    assert(Compute_CRC16(record, 12) == ((uint16_t)record[12] | ((uint16_t)record[13] << 8)));
    /* Multiple wraps exercise sector erases and recovery scanning. */
    for (unsigned i = 0; i < 1200; ++i) assert(Logger_WriteRecord(EVENT_FAULT, dtc, 422, 67) == 0);
    Logger_Init();
    assert(Logger_WriteRecord(EVENT_BOOT, dtc, 423, 68) == 0);
    puts("PASS: CRC, page crossing, NOR programming, sector crossing, torn slot, wraps");
}
