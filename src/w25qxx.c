#include "w25qxx.h"
#include <stdio.h>

/**
 * Initialize SPI and verify communication with the W25Qxx Flash chip by reading its Device ID.
 */
void W25Q_Init(void) {
    SPI_Init();
    
    // Read and verify device ID
    uint16_t device_id = W25Q_ReadDeviceID();
    printf("[W25Qxx-Init] Querying Winbond Chip ID... Expected: 0xEF17, Received: 0x%04X\n", device_id);
    
    if (device_id == 0xEF17) {
        printf("[W25Qxx-Init] W25Q128 Flash detected and ready.\n");
    } else {
        printf("[W25Qxx-Init] ERROR: Unknown Flash Device ID!\n");
    }
}

/**
 * Reads the Status Register 1 of the Flash chip.
 * Bit 0 is the BUSY flag. Bit 1 is the WEL (Write Enable Latch) flag.
 */
uint8_t W25Q_ReadStatus(void) {
    uint8_t status = 0;
    
    SPI_CS_Select();
    SPI_TransmitReceive(W25Q_READ_STATUS_REG1);
    status = SPI_TransmitReceive(0x00); // Send dummy byte to shift out status
    SPI_CS_Deselect();
    
    return status;
}

/**
 * Sends the Write Enable (0x06) instruction to the Flash chip.
 * Must be executed before every erase or program operation.
 */
void W25Q_WriteEnable(void) {
    SPI_CS_Select();
    SPI_TransmitReceive(W25Q_WRITE_ENABLE);
    SPI_CS_Deselect();
}

/**
 * Block the CPU until the internal write/erase cycle of the Flash completes.
 * Polls status register 1 until the BUSY bit (bit 0) becomes 0.
 */
void W25Q_WaitBusy(void) {
    while (W25Q_ReadStatus() & 0x01) {
        // Poll status register
    }
}

/**
 * Erases a 4KB sector of Flash memory starting at sector_addr.
 * Wipes the sector, setting all bytes to 0xFF.
 */
void W25Q_SectorErase(uint32_t sector_addr) {
    W25Q_WriteEnable();
    W25Q_WaitBusy();
    
    SPI_CS_Select();
    SPI_TransmitReceive(W25Q_SECTOR_ERASE);
    
    // Send 24-bit address (MSB first)
    SPI_TransmitReceive((sector_addr >> 16) & 0xFF);
    SPI_TransmitReceive((sector_addr >> 8) & 0xFF);
    SPI_TransmitReceive(sector_addr & 0xFF);
    
    SPI_CS_Deselect();
    W25Q_WaitBusy(); // Block until hardware erase complete
}

/**
 * Programs a page of up to 256 bytes in Flash starting at page_addr.
 * Target address range must be erased (0xFF) beforehand.
 */
void W25Q_PageProgram(uint32_t page_addr, uint8_t *data, uint16_t size) {
    W25Q_WriteEnable();
    W25Q_WaitBusy();
    
    SPI_CS_Select();
    SPI_TransmitReceive(W25Q_PAGE_PROGRAM);
    
    // Send 24-bit address
    SPI_TransmitReceive((page_addr >> 16) & 0xFF);
    SPI_TransmitReceive((page_addr >> 8) & 0xFF);
    SPI_TransmitReceive(page_addr & 0xFF);
    
    // Shift out data array
    for (uint16_t i = 0; i < size; i++) {
        SPI_TransmitReceive(data[i]);
    }
    
    SPI_CS_Deselect();
    W25Q_WaitBusy(); // Block until hardware program complete
}

/**
 * Reads a block of data sequentially starting from target address.
 */
void W25Q_ReadData(uint32_t addr, uint8_t *buffer, uint32_t size) {
    SPI_CS_Select();
    SPI_TransmitReceive(W25Q_READ_DATA);
    
    // Send 24-bit address
    SPI_TransmitReceive((addr >> 16) & 0xFF);
    SPI_TransmitReceive((addr >> 8) & 0xFF);
    SPI_TransmitReceive(addr & 0xFF);
    
    // Shift in data bytes sequentially
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = SPI_TransmitReceive(0x00);
    }
    
    SPI_CS_Deselect();
}

/**
 * Reads the Winbond Manufacturer and Device ID over SPI.
 * Expected return value is 0xEF17 (Manufacturer ID 0xEF, Device ID 0x17).
 */
uint16_t W25Q_ReadDeviceID(void) {
    uint8_t mfg_id = 0;
    uint8_t dev_id = 0;
    
    SPI_CS_Select();
    SPI_TransmitReceive(W25Q_DEVICE_ID);
    
    // 24-bit dummy address
    SPI_TransmitReceive(0x00);
    SPI_TransmitReceive(0x00);
    SPI_TransmitReceive(0x00);
    
    mfg_id = SPI_TransmitReceive(0x00);
    dev_id = SPI_TransmitReceive(0x00);
    
    SPI_CS_Deselect();
    
    return (uint16_t)((mfg_id << 8) | dev_id);
}
