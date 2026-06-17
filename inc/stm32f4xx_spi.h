#ifndef STM32F4XX_SPI_H
#define STM32F4XX_SPI_H

#include <stdint.h>

/* Mocked Register Structures for STM32F4 SPI Peripherals */

typedef struct {
    volatile uint32_t MODER;    /* GPIO port mode register */
    volatile uint32_t ODR;      /* GPIO port output data register */
    volatile uint32_t BSRR;     /* GPIO port bit set/reset register */
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CR1;      /* SPI control register 1 */
    volatile uint32_t CR2;      /* SPI control register 2 */
    volatile uint32_t SR;       /* SPI status register */
    volatile uint32_t DR;       /* SPI data register */
} SPI_TypeDef;

extern GPIO_TypeDef *GPIOA;
extern SPI_TypeDef *SPI1;

/* Bit Definitions to match real STM32 SPI headers */
#define SPI_CR1_SPE         ((uint16_t)0x0040)  /* SPI enable */
#define SPI_CR1_MSTR        ((uint16_t)0x0004)  /* Master configuration */
#define SPI_CR1_CPOL        ((uint16_t)0x0002)  /* Clock polarity */
#define SPI_CR1_CPHA        ((uint16_t)0x0001)  /* Clock phase */
#define SPI_CR1_SSM         ((uint16_t)0x0200)  /* Software slave management */
#define SPI_CR1_SSI         ((uint16_t)0x0100)  /* Internal slave select */
#define SPI_CR1_DFF         ((uint16_t)0x0800)  /* Data frame format (8-bit or 16-bit) */

#define SPI_SR_TXE          ((uint16_t)0x0002)  /* Transmit buffer empty */
#define SPI_SR_RXNE         ((uint16_t)0x0001)  /* Receive buffer not empty */
#define SPI_SR_BSY          ((uint16_t)0x0080)  /* Busy flag */

#endif /* STM32F4XX_SPI_H */
