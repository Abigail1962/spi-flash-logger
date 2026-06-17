#ifndef W25QXX_H
#define W25QXX_H

#include "stm32f4xx_spi.h"

/* W25Qxx Instruction Set */
#define W25Q_WRITE_ENABLE       0x06
#define W25Q_WRITE_DISABLE      0x04
#define W25Q_READ_STATUS_REG1   0x05
#define W25Q_READ_DATA          0x03
#define W25Q_PAGE_PROGRAM       0x02
#define W25Q_SECTOR_ERASE       0x20
#define W25Q_DEVICE_ID          0x90

/* SPI Driver Prototypes */
void SPI_Init(void);
uint8_t SPI_TransmitReceive(uint8_t data);
void SPI_CS_Select(void);
void SPI_CS_Deselect(void);

/* W25Qxx Driver Prototypes */
void W25Q_Init(void);
uint8_t W25Q_ReadStatus(void);
void W25Q_WriteEnable(void);
void W25Q_WaitBusy(void);
void W25Q_SectorErase(uint32_t sector_addr);
void W25Q_PageProgram(uint32_t page_addr, uint8_t *data, uint16_t size);
void W25Q_ReadData(uint32_t addr, uint8_t *buffer, uint32_t size);
uint16_t W25Q_ReadDeviceID(void);

#endif /* W25QXX_H */
