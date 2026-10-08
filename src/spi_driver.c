#include "w25qxx.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Mock Allocations for SPI1 & GPIOA Peripherals */
static SPI_TypeDef mock_spi1;
static GPIO_TypeDef mock_gpioa;

SPI_TypeDef *SPI1 = &mock_spi1;
GPIO_TypeDef *GPIOA = &mock_gpioa;

/* ==============================================================================
 * WINBOND W25Q128 FLASH HARDWARE CHIP SIMULATOR
 * ==============================================================================
 */
#define SIM_FLASH_SIZE (16384) // 16KB mock array representing first 4 sectors of Flash
static uint8_t g_sim_flash_array[SIM_FLASH_SIZE];
static int initialized;
static int forced_busy;
void SPI_SimSetBusy(int enabled) { forced_busy = enabled; }
static uint32_t page_base;

/* Flash Chip State variables */
typedef enum {
    FLASH_STATE_IDLE = 0,
    FLASH_STATE_CMD,
    FLASH_STATE_ADDR1,
    FLASH_STATE_ADDR2,
    FLASH_STATE_ADDR3,
    FLASH_STATE_READ_DATA,
    FLASH_STATE_WRITE_DATA,
    FLASH_STATE_ERASE_WAIT
} FlashSimState_t;

static FlashSimState_t g_sim_state = FLASH_STATE_IDLE;
static uint8_t g_active_cmd = 0;
static uint32_t g_transfer_address = 0;
static uint32_t g_addr_byte_count = 0;
static uint8_t g_status_reg1 = 0;
static uint8_t g_write_enabled = 0;

static void Reset_Flash_State_Machine(void) {
    g_sim_state = FLASH_STATE_CMD;
    g_transfer_address = 0;
    g_addr_byte_count = 0;
}

/**
 * Byte-level simulation of Winbond W25Q128 controller state machine.
 * Process bytes shifted in over MOSI and returns bytes shifted out over MISO.
 */
static uint8_t Simulate_Flash_Byte_Transfer(uint8_t mosi) {
    uint8_t miso = 0x00;

    switch (g_sim_state) {
        case FLASH_STATE_CMD:
            g_active_cmd = mosi;

            if (g_active_cmd == W25Q_WRITE_ENABLE) {
                g_write_enabled = 1;
                g_status_reg1 |= 0x02; // Set WEL (Write Enable Latch) bit
                g_sim_state = FLASH_STATE_IDLE;
            }
            else if (g_active_cmd == W25Q_WRITE_DISABLE) {
                g_write_enabled = 0;
                g_status_reg1 &= ~0x02; // Clear WEL bit
                g_sim_state = FLASH_STATE_IDLE;
            }
            else if (g_active_cmd == W25Q_READ_STATUS_REG1) {
                miso = g_status_reg1 | (forced_busy ? 1U : 0U);
                // Keep returning status if master continues clocks
                g_sim_state = FLASH_STATE_READ_DATA;
            }
            else if (g_active_cmd == W25Q_READ_DATA ||
                     g_active_cmd == W25Q_PAGE_PROGRAM ||
                     g_active_cmd == W25Q_SECTOR_ERASE) {
                g_sim_state = FLASH_STATE_ADDR1;
            }
            else if (g_active_cmd == W25Q_DEVICE_ID) {
                g_sim_state = FLASH_STATE_ADDR1; // Device ID requires 3 dummy address bytes first
            }
            else {
                g_sim_state = FLASH_STATE_IDLE;
            }
            break;

        case FLASH_STATE_ADDR1:
            g_transfer_address = (mosi << 16);
            g_sim_state = FLASH_STATE_ADDR2;
            break;

        case FLASH_STATE_ADDR2:
            g_transfer_address |= (mosi << 8);
            g_sim_state = FLASH_STATE_ADDR3;
            break;

        case FLASH_STATE_ADDR3:
            g_transfer_address |= mosi;

            if (g_active_cmd == W25Q_READ_DATA) {
                g_sim_state = FLASH_STATE_READ_DATA;
            }
            else if (g_active_cmd == W25Q_PAGE_PROGRAM) {
                if (g_write_enabled) {
                    page_base = g_transfer_address & ~255U;
                    g_sim_state = FLASH_STATE_WRITE_DATA;
                    g_status_reg1 |= 0x01; // Set BUSY bit
                } else {
                    g_sim_state = FLASH_STATE_IDLE; // Ignored without Write Enable
                }
            }
            else if (g_active_cmd == W25Q_SECTOR_ERASE) {
                if (g_write_enabled) {
                    // Erase 4KB sector: fill sector with 0xFF (erased state)
                    uint32_t sector_start = g_transfer_address & ~0xFFF; // Align to 4KB boundary
                    if (sector_start + 4096 <= SIM_FLASH_SIZE) {
                        memset(&g_sim_flash_array[sector_start], 0xFF, 4096);
                    }
                    g_status_reg1 |= 0x01; // Set BUSY bit
                    g_sim_state = FLASH_STATE_ERASE_WAIT;
                } else {
                    g_sim_state = FLASH_STATE_IDLE;
                }
            }
            else if (g_active_cmd == W25Q_DEVICE_ID) {
                g_sim_state = FLASH_STATE_READ_DATA;
            }
            break;

        case FLASH_STATE_READ_DATA:
            if (g_active_cmd == W25Q_READ_DATA) {
                if (g_transfer_address < SIM_FLASH_SIZE) {
                    miso = g_sim_flash_array[g_transfer_address];
                } else {
                    miso = 0xFF;
                }
                g_transfer_address++; // Auto-increment address for sequential reads
            }
            else if (g_active_cmd == W25Q_READ_STATUS_REG1) {
                miso = g_status_reg1 | (forced_busy ? 1U : 0U);
            }
            else if (g_active_cmd == W25Q_DEVICE_ID) {
                // Returns Winbond Manufacturer ID (0xEF) followed by Device ID (0x17 for W25Q128)
                if (g_addr_byte_count == 0) {
                    miso = 0xEF;
                    g_addr_byte_count++;
                } else {
                    miso = 0x17;
                }
            }
            break;

        case FLASH_STATE_WRITE_DATA:
            if (g_active_cmd == W25Q_PAGE_PROGRAM) {
                if (g_transfer_address < SIM_FLASH_SIZE) {
                    g_sim_flash_array[g_transfer_address] &= mosi; /* NOR can only program 1 to 0. */
                }
                g_transfer_address = page_base | ((g_transfer_address + 1U) & 255U);
            }
            break;

        case FLASH_STATE_ERASE_WAIT:
            // Waiting for CS to go high to complete operation
            break;

        default:
            miso = 0xFF;
            break;
    }

    return miso;
}

static void Terminate_Flash_Transaction(void) {
    // When CS goes high, internal program/erase finishes (clear BUSY)
    if (g_active_cmd == W25Q_PAGE_PROGRAM || g_active_cmd == W25Q_SECTOR_ERASE)
        g_status_reg1 &= ~0x01; // Complete mock write/erase only

    // WEL and Write Enable are cleared after Page Program or Sector Erase completes
    if (g_active_cmd == W25Q_PAGE_PROGRAM || g_active_cmd == W25Q_SECTOR_ERASE) {
        g_status_reg1 &= ~0x02; // Clear WEL
        g_write_enabled = 0;
    }
    g_sim_state = FLASH_STATE_IDLE;
}

/* ==============================================================================
 * REGISTER-LEVEL SPI DRIVER IMPLEMENTATION
 * ==============================================================================
 */

/**
 * Initialize SPI1 in Master Mode, 8-bit frame, Mode 0 (CPOL=0, CPHA=0).
 */
void SPI_Init(void) {
    if (!initialized) { memset(g_sim_flash_array, 0xFF, sizeof(g_sim_flash_array)); initialized = 1; }
    /* 1. Configure Control Register 1:
     * - Enable SPI Peripheral (SPE)
     * - Configure as Master Device (MSTR)
     * - Enable Software Slave Management (SSM & SSI)
     * - Frame format 8-bit (DFF cleared)
     * - Clock Mode 0: CPOL=0, CPHA=0
     */
    SPI1->CR1 = 0;
    SPI1->CR1 |= SPI_CR1_SPE;   // Enable peripheral
    SPI1->CR1 |= SPI_CR1_MSTR;  // Configure as Master mode
    SPI1->CR1 |= SPI_CR1_SSM;   // Software slave management enable
    SPI1->CR1 |= SPI_CR1_SSI;   // Internal slave select
}

/**
 * Select the external Flash chip by asserting the GPIO CS Pin LOW.
 */
void SPI_CS_Select(void) {
    GPIOA->BSRR = (1 << (4 + 16));  // Reset GPIOA Pin 4 LOW (Assert CS)
    GPIOA->ODR &= ~(1 << 4);        // Update ODR for simulation check

    Reset_Flash_State_Machine();    // Hardware event: CS falling edge resets controller
}

/**
 * Deselect the external Flash chip by de-asserting the GPIO CS Pin HIGH.
 */
void SPI_CS_Deselect(void) {
    GPIOA->BSRR = (1 << 4);         // Set GPIOA Pin 4 HIGH (De-assert CS)
    GPIOA->ODR |= (1 << 4);         // Update ODR for simulation check

    Terminate_Flash_Transaction();  // Hardware event: CS rising edge terminates write/erase
}

/**
 * Transmits a byte over SPI MOSI and simultaneously receives a byte over MISO.
 * Uses status register polling to wait for TXE (Transmit Empty) and RXNE (Receive Not Empty).
 */
uint8_t SPI_TransmitReceive(uint8_t data) {
    // Write data to data register (triggers clock cycles)
    SPI1->DR = data;

    // Wait until Transmit buffer is Empty (TXE flag)
    SPI1->SR |= SPI_SR_TXE; // Mock hardware flag status
    while (!(SPI1->SR & SPI_SR_TXE)) {
        /* Wait for TXE to set */
    }

    // In this simulation, we feed MOSI byte into the simulated Flash controller
    // and retrieve the MISO byte.
    uint8_t rx_miso = Simulate_Flash_Byte_Transfer(data);
    SPI1->DR = rx_miso; // Populate receive data register

    // Wait until Receive buffer is Not Empty (RXNE flag)
    SPI1->SR |= SPI_SR_RXNE; // Mock hardware flag status
    while (!(SPI1->SR & SPI_SR_RXNE)) {
        /* Wait for RXNE to set */
    }

    return (uint8_t)SPI1->DR;
}
