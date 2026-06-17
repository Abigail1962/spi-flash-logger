#include "logger.h"
#include "w25qxx.h"
#include <stdio.h>
#include <string.h>

static uint32_t g_write_ptr = FLASH_LOG_START;
static uint32_t g_boot_time = 0;

/**
 * Standard CRC-16 CCITT (Polynomial 0x1021, Seed 0xFFFF) computation.
 * Used to verify integrity of log records before processing them on boot.
 */
uint16_t Compute_CRC16(const uint8_t *data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * Scan W25Qxx Flash sectors to locate the next available write slot.
 * Starts from FLASH_LOG_START and increments by LOG_RECORD_SIZE (14 bytes).
 * If all slots are full, defaults to wrapping around to FLASH_LOG_START.
 */
void Logger_Init(void) {
    W25Q_Init();
    
    printf("[FlashLogger] Scanning Flash logging sectors to restore write pointer...\n");
    
    uint8_t buffer[LOG_RECORD_SIZE];
    uint32_t addr = FLASH_LOG_START;
    uint8_t found = 0;

    while (addr < FLASH_LOG_END) {
        // Read 14-byte slot
        W25Q_ReadData(addr, buffer, LOG_RECORD_SIZE);
        
        LogRecord_t *record = (LogRecord_t *)buffer;
        
        // Check if the slot is empty (all bytes 0xFF in erased Flash)
        if (record->timestamp == 0xFFFFFFFF) {
            g_write_ptr = addr;
            found = 1;
            printf("[FlashLogger] Found next empty slot at address: 0x%08X\n", g_write_ptr);
            break;
        }
        
        // If slot contains data, verify CRC-16 integrity
        uint16_t calc_crc = Compute_CRC16(buffer, 12); // First 12 bytes
        if (calc_crc != record->crc) {
            // Found a corrupted slot or boundary anomaly, treat as write destination
            g_write_ptr = addr;
            found = 1;
            printf("[FlashLogger] WARNING: CRC mismatch at 0x%08X (calc: 0x%04X, rec: 0x%04X). Resetting write pointer here.\n", 
                addr, calc_crc, record->crc);
            break;
        }

        addr += LOG_RECORD_SIZE;
        
        // Prevent sector overflow into unallocated regions
        if ((addr + LOG_RECORD_SIZE) > FLASH_LOG_END) {
            break;
        }
    }

    if (!found) {
        // Logging memory is completely full, wrap around to start
        g_write_ptr = FLASH_LOG_START;
        printf("[FlashLogger] Logging array is FULL. Wrapping write pointer back to 0x%08X\n", g_write_ptr);
    }
}

/**
 * Writes a Diagnostic Record into Flash.
 * Handles boundary sector erases and circular buffer wrap-around.
 */
int Logger_WriteRecord(uint8_t event_type, const uint8_t *dtc, uint16_t voltage, uint8_t temperature) {
    LogRecord_t record;
    
    record.timestamp = g_boot_time++; // Simulated time progression
    record.event_type = event_type;
    memcpy(record.dtc, dtc, 3);
    record.voltage = voltage;
    record.temperature = temperature;
    record.padding = 0x00;
    
    // Calculate CRC-16 for data integrity
    uint8_t raw_buffer[LOG_RECORD_SIZE];
    memcpy(raw_buffer, &record, 12);
    record.crc = Compute_CRC16(raw_buffer, 12);

    /* Flash Constraint Management:
     * 1. Check if the current write pointer crosses a 4KB sector boundary.
     * 2. If yes, we must erase the sector first before programming new data.
     */
    if (g_write_ptr % SECTOR_SIZE == 0) {
        printf("[FlashLogger] Sector boundary crossed. Erasing 4KB Sector at 0x%08X...\n", g_write_ptr);
        W25Q_SectorErase(g_write_ptr);
    }

    // Program the 14-byte record to Flash
    printf("[FlashLogger] Programming log record to 0x%08X (Timestamp: %ds, DTC: 0x%02X%02X%02X)\n", 
        g_write_ptr, record.timestamp, record.dtc[0], record.dtc[1], record.dtc[2]);
        
    W25Q_PageProgram(g_write_ptr, (uint8_t *)&record, LOG_RECORD_SIZE);

    // Increment write pointer
    g_write_ptr += LOG_RECORD_SIZE;

    // Handle circular wrap around
    if (g_write_ptr + LOG_RECORD_SIZE > FLASH_LOG_END) {
        g_write_ptr = FLASH_LOG_START;
        printf("[FlashLogger] Write boundary reached. Wrapping around to 0x%08X\n", g_write_ptr);
    }

    return 0;
}

/**
 * Read and decode all valid log records written in the circular buffer.
 */
void Logger_PrintAllRecords(void) {
    uint8_t buffer[LOG_RECORD_SIZE];
    uint32_t addr = FLASH_LOG_START;
    uint32_t records_read = 0;

    printf("\n--- Reading Diagnostic Trouble Logs from Flash ---\n");
    printf("| Address    | Time | Event Type  | DTC Code   | Voltage | Temp |\n");
    printf("|------------|------|-------------|------------|---------|------|\n");

    while (addr < FLASH_LOG_END) {
        W25Q_ReadData(addr, buffer, LOG_RECORD_SIZE);
        LogRecord_t *record = (LogRecord_t *)buffer;
        
        if (record->timestamp == 0xFFFFFFFF) {
            // Empty slot, stop printing
            break;
        }

        // Verify CRC
        uint16_t calc_crc = Compute_CRC16(buffer, 12);
        if (calc_crc == record->crc) {
            const char *event_name = (record->event_type == EVENT_BOOT) ? "BOOT" :
                                     (record->event_type == EVENT_FAULT) ? "FAULT" : "SHUTDOWN";
                                     
            printf("| 0x%08X | %4ds | %-11s | 0x%02X%02X%02X   | %5.1fV  | %3dC |\n",
                addr,
                record->timestamp,
                event_name,
                record->dtc[0], record->dtc[1], record->dtc[2],
                record->voltage / 10.0f,
                record->temperature - 40
            );
            records_read++;
        } else {
            // Mismatch indicating end of valid logs or corrupted memory
            break;
        }

        addr += LOG_RECORD_SIZE;
        if (addr + LOG_RECORD_SIZE > FLASH_LOG_END) {
            break;
        }
    }
    printf("--- Read complete. Total valid records recovered: %d ---\n\n", records_read);
}
