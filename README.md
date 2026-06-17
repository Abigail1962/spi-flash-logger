# STM32 SPI Flash Driver & Diagnostic Fault Logger

This project is a bare-metal C firmware application for an ARM Cortex-M (STM32F4 series) microcontroller. It implements a custom, register-level driver for a Winbond W25Q series (e.g. W25Q128) SPI Flash memory chip, and builds a diagnostic fault and telemetry logger with power-loss recovery.

The codebase includes a Software-in-the-Loop (SIL) simulation harness allowing it to compile and run on macOS/Linux using standard gcc tooling, validating the driver transaction protocol, flash sector management, and boot-up log recovery.

---

## 🛠️ Peripheral Register-Level Architecture

The project features direct register manipulation for STM32 hardware blocks:

1. **Direct Register SPI Driver (`src/spi_driver.c`)**:
   - Configures `SPI1` control registers: Master Mode, 8-bit Data Frame, Baud rate prescaler, Clock Polarity (`CPOL`=0), and Clock Phase (`CPHA`=0) matching SPI Mode 0 for Winbond Flash chips.
   - Polling-based byte transfer checks status registers (`TXE` and `RXNE` flags) to manage the shift register.
   - Direct GPIO chip-select (`CS` pin) toggling using the Bit Set/Reset Register `BSRR` on `GPIOA` Pin 4.

2. **W25Qxx Flash Driver (`src/w25qxx.c`)**:
   - Implements the SPI instruction set command protocol:
     - Sector Erase (`0x20`): Wipes a 4KB sector, resetting all bytes to `0xFF`.
     - Page Program (`0x02`): Writes byte arrays into memory pages.
     - Read Data (`0x03`): Sequentially reads memory bytes.
     - Read Status Register 1 (`0x05`): Polls the hardware `BUSY` flag (bit 0) to block the host CPU until write/erase cycles complete.
     - Write Enable (`0x06`): Sets the Write Enable Latch (WEL) bit.

3. **Diagnostic circular Logger (`src/logger.c`)**:
   - Structs logs with timestamps, event types, 3-byte DTC codes, pack voltage, max temperatures, and a **CRC-16 CCITT checksum** (polynomial `0x1021`) for data integrity.
   - **Flash Constraint Handling**: Enforces that a 4KB sector must be erased before programming new data when crossing page/sector boundaries.
   - **Circular Logging**: Automatically erases and wraps around to `FLASH_LOG_START` upon hitting the allocated memory boundary (`FLASH_LOG_END`).
   - **Boot Recovery Scan**: Scans Flash sectors during startup, verifies CRCs, and dynamically restores the write pointer exactly where it was before power shutdown.

---

## 📁 File Structure

- **`inc/stm32f4xx_spi.h`**: Mappings of STM32 SPI and GPIO registers.
- **`inc/w25qxx.h`**: Winbond W25Qxx commands and driver prototypes.
- **`inc/logger.h`**: Log structure, CRC functions, and diagnostic configurations.
- **`src/spi_driver.c`**: SPI register driver and simulated W25Q128 controller state machine.
- **`src/w25qxx.c`**: Flash command protocol implementations.
- **`src/logger.c`**: Circular log sector packer and CRC-16 CCITT checker.
- **`src/main.c`**: Test harness verifying boot scans, logging, power-outage recovery, and log retrieval.
- **`Makefile`**: Build configuration.

---

## ⚙️ Compilation & Running

### Compile the Simulator
```bash
make
```

### Run the Simulation
```bash
./spi_logger_sim
```

### Clean Build Outputs
```bash
make clean
```

---

## 📊 Simulation Demo Output

Executing the binary (`./spi_logger_sim`) demonstrates how the host successfully recovers previously written logs after a cold power outage:

```text
==================================================================
     STM32 BARE-METAL SPI FLASH DIAGNOSTIC LOGGER SIMULATION      
==================================================================

--- [BOOT CYCLE 1] Powering up host and initializing Flash... ---
[W25Qxx-Init] Querying Winbond Chip ID... Expected: 0xEF17, Received: 0xEF17
[W25Qxx-Init] W25Q128 Flash detected and ready.
[FlashLogger] Scanning Flash logging sectors to restore write pointer...
[FlashLogger] WARNING: CRC mismatch at 0x00000000 (calc: 0x84F9, rec: 0x0000). Resetting write pointer here.

[BMS-System] System running. Logging initial boot events...
[FlashLogger] Sector boundary crossed. Erasing 4KB Sector at 0x00000000...
[FlashLogger] Programming log record to 0x00000000 (Timestamp: 0s, DTC: 0x000000)

[BMS-System] Injecting high-voltage cell undervoltage fault...
[FlashLogger] Programming log record to 0x0000000E (Timestamp: 1s, DTC: 0x9020A2)

[BMS-System] Injecting critical pack over-temperature fault...
[FlashLogger] Programming log record to 0x0000001C (Timestamp: 2s, DTC: 0x9010A1)

[BMS-System] Shutting down charging system normally...
[FlashLogger] Programming log record to 0x0000002A (Timestamp: 3s, DTC: 0x000000)

--- Reading Diagnostic Trouble Logs from Flash ---
| Address    | Time | Event Type  | DTC Code   | Voltage | Temp |
|------------|------|-------------|------------|---------|------|
| 0x00000000 |    0s | BOOT        | 0x000000   |  42.0V  |  25C |
| 0x0000000E |    1s | FAULT       | 0x9020A2   |  33.6V  |  28C |
| 0x0000001C |    2s | FAULT       | 0x9010A1   |  41.5V  |  65C |
| 0x0000002A |    3s | SHUTDOWN    | 0x000000   |  41.8V  |  24C |
--- Read complete. Total valid records recovered: 4 ---

------------------------------------------------------------------
⚠️  [SYSTEM EVENT] Simulated Unplanned Power Outage (Cold Reset)   
------------------------------------------------------------------

--- [BOOT CYCLE 2] Re-powering host and reloading firmware... ---
[W25Qxx-Init] Querying Winbond Chip ID... Expected: 0xEF17, Received: 0xEF17
[W25Qxx-Init] W25Q128 Flash detected and ready.
[FlashLogger] Scanning Flash logging sectors to restore write pointer...
[FlashLogger] Found next empty slot at address: 0x00000038

[BMS-System] System restored. Logging recovery events...
[FlashLogger] Programming log record to 0x00000038 (Timestamp: 4s, DTC: 0x000000)

--- Reading Diagnostic Trouble Logs from Flash ---
| Address    | Time | Event Type  | DTC Code   | Voltage | Temp |
|------------|------|-------------|------------|---------|------|
| 0x00000000 |    0s | BOOT        | 0x000000   |  42.0V  |  25C |
| 0x0000000E |    1s | FAULT       | 0x9020A2   |  33.6V  |  28C |
| 0x0000001C |    2s | FAULT       | 0x9010A1   |  41.5V  |  65C |
| 0x0000002A |    3s | SHUTDOWN    | 0x000000   |  41.8V  |  24C |
| 0x00000038 |    4s | BOOT        | 0x000000   |  42.1V  |  26C |
--- Read complete. Total valid records recovered: 5 ---
```
