#include <stdio.h>
#include <assert.h>
#include "logger.h"

int main(void) {
    printf("==================================================================\n");
    printf("     STM32 BARE-METAL SPI FLASH DIAGNOSTIC LOGGER SIMULATION      \n");
    printf("==================================================================\n\n");

    /* ==============================================================================
     * BOOT CYCLE 1: Cold Boot with Clean Flash Memory
     * ==============================================================================
     */
    printf("--- [BOOT CYCLE 1] Powering up host and initializing Flash... ---\n");
    assert(Logger_Init() == 0); // Configures SPI and scans empty Flash

    printf("\n[BMS-System] System running. Logging initial boot events...\n");
    // Write boot event: no active DTC, voltage 42.0V, temp 25C (offset 40 -> 65)
    uint8_t no_dtc[3] = {0x00, 0x00, 0x00};
    assert(Logger_WriteRecord(EVENT_BOOT, no_dtc, 420, 65) == 0);

    printf("\n[BMS-System] Injecting high-voltage cell undervoltage fault...\n");
    // Write cell undervoltage fault event: DTC 0x9020A2, voltage 33.6V, temp 28C (offset 40 -> 68)
    uint8_t undervolt_dtc[3] = {0x90, 0x20, 0xA2};
    assert(Logger_WriteRecord(EVENT_FAULT, undervolt_dtc, 336, 68) == 0);

    printf("\n[BMS-System] Injecting critical pack over-temperature fault...\n");
    // Write overtemp fault event: DTC 0x9010A1, voltage 41.5V, temp 65C (offset 40 -> 105)
    uint8_t overtemp_dtc[3] = {0x90, 0x10, 0xA1};
    assert(Logger_WriteRecord(EVENT_FAULT, overtemp_dtc, 415, 105) == 0);

    printf("\n[BMS-System] Shutting down charging system normally...\n");
    assert(Logger_WriteRecord(EVENT_SHUTDOWN, no_dtc, 418, 64) == 0);

    // Print all logs in Flash currently
    Logger_PrintAllRecords();

    /* ==============================================================================
     * CRITICAL EVENT: Simulated Power Loss / Host logger reinitialization
     * ==============================================================================
     */
    printf("------------------------------------------------------------------\n");
    printf("⚠️  [SYSTEM EVENT] Simulated Host reinitialization (no interrupted write) (Host logger reinitialization)   \n");
    printf("------------------------------------------------------------------\n\n");

    /* ==============================================================================
     * BOOT CYCLE 2: Recovery Boot
     * ==============================================================================
     */
    printf("--- [BOOT CYCLE 2] Re-powering host and reloading firmware... ---\n");

    // Logger_Init must scan the Flash sectors, verify CRCs, and locate the next empty slot
    // without erasing the logs we wrote in Boot Cycle 1!
    assert(Logger_Init() == 0);

    printf("\n[BMS-System] System restored. Logging recovery events...\n");
    // Log new boot event showing recovery status
    assert(Logger_WriteRecord(EVENT_BOOT, no_dtc, 421, 66) == 0);

    // Print all recovered records, proving Cycle 1 data is preserved and Cycle 2 data was appended
    Logger_PrintAllRecords();

    return 0;
}
