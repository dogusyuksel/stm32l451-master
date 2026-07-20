#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp.h"
#include "uart_handlers.h"
#include "main.h"
#include "usart.h"
#include "rng.h"
#include "spi.h"
#include "i2c.h"
#include "iwdg.h"
#include "adc.h"
#include "logging.h"
#include "stm32l4xx_hal.h"
#include "cm_backtrace.h"

static uint8_t uart1_rx_byte;
static uint8_t uart3_rx_byte;

uint8_t xmodem_buffer[2048] = {0};
static uint32_t xmodem_buffer_counter = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart1) {
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart1_rx_byte, 1);
        custom_uart_interrupt_handler(uart1_rx_byte);
    } else if (huart == &huart3) {
        HAL_UART_Receive_IT(&huart3, (uint8_t *)&uart3_rx_byte, 1);
        xmodem_buffer[xmodem_buffer_counter++] = uart3_rx_byte;
        if (xmodem_buffer_counter >= sizeof(xmodem_buffer)) {
            xmodem_buffer_counter = 0;
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == BUTTON_Pin) {
    log_debug("Button Pressed!\n\r");
  }
}

static void uart_send(uint8_t *buffer, uint32_t len) {
  HAL_UART_Transmit(&huart1, (uint8_t *)buffer, len, len);
}

void board_init(void) {
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart1_rx_byte, 1); // arm uart it

  logging_init(uart_send, LEVEL_DEBUG);

  cm_backtrace_init("stm32l451-master", "NA", VERSION);

  log_debug("Board initialized\n\r");
}

static void RNG_test(void) {
  uint32_t random_value = 0;
  if (HAL_RNG_GenerateRandomNumber(&hrng, &random_value) == HAL_OK) {
      log_debug("RNG: %lu\r\n", random_value);
  } else {
      log_debug("RNG ERROR\r\n");
  }
}

#define BLS_CODE_MDID 0x9F /**< Manufacturer Device ID */
#define BLS_CODE_DP 0xB9  /**< Power down */
#define BLS_CODE_RDP 0xAB /**< Power standby */

static void extFlashSelect(void) {
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
}

static void extFlashDeselect(void) {
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
}

static int extFlashPowerStandby(void) {
  uint8_t cmd = BLS_CODE_RDP;
  int ret = HAL_ERROR;

  extFlashSelect();
  ret = HAL_SPI_Transmit(&hspi1, &cmd, 1, 200);
  extFlashDeselect();

  return ret;
}

static int ExtFlash_readInfo(void) {
  int ret = HAL_OK;

  uint8_t tx_buf[4] = { BLS_CODE_MDID, 0x00, 0x00, 0x00 }; // 1 command + 3 dummy
  uint8_t rx_buf[4] = {0};
  extFlashSelect();
  HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 4, 200);
  extFlashDeselect();

  log_debug("spi1 rx_buf: %X %X %X\r\n",
        rx_buf[1], rx_buf[2], rx_buf[3]);

  return ret;
}

static int extFlashPowerDown(void) {
  uint8_t cmd = BLS_CODE_DP;
  int ret = HAL_ERROR;

  extFlashSelect();
  ret = HAL_SPI_Transmit(&hspi1, &cmd, 1, 200);
  extFlashDeselect();

  return ret;
}

static uint16_t read_adc(void) {
        ADC_ChannelConfTypeDef conf = {
        .Channel = ADC_CHANNEL_5,
        .Rank = 1,
        .SamplingTime = ADC_SAMPLETIME_24CYCLES_5,
    };
    if (HAL_ADC_ConfigChannel(&hadc1, &conf) != HAL_OK) {
        Error_Handler();
    }

    // if continuous mode selected, you should not have to call it each time as well as HAL_ADC_Stop function
    // HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 500);
    uint16_t adc_result = HAL_ADC_GetValue(&hadc1);
    uint16_t adc_result_in_mv = ((3300 * adc_result) / 4095);
    // HAL_ADC_Stop(&hadc1);

    return adc_result_in_mv;
}

void board_periodic_task(void *argument) {
//   static uint32_t led_toggle_counter = 0;

//   /* Put the part is standby mode */
//   extFlashPowerStandby();
//   osDelay(20);
//   /* Verify manufacturer and device ID */
//   ExtFlash_readInfo();
//   osDelay(20);
//   // Put the part in low power mode
//   extFlashPowerDown();

//   uint8_t buf[2];
//   uint8_t chipID = 0;
//   buf[0] = 0xFC; // SendingData state
//   buf[1] = 0x00; // Chip ID register

//   HAL_I2C_Master_Transmit(&hi2c1, 0x6B << 1, buf, 2, 300);
//   HAL_I2C_Master_Receive(&hi2c1, 0x6B << 1, &chipID, 1, 300);

//   log_debug("i2c1, BMA180 chipID: %u\n", chipID);

  /* Infinite loop */
  for(;;) {
    // custom_uart_handler(osKernelGetTickCount());
    // osDelay(UART_RX_TIMEOUT_MS);

    // if ((++(led_toggle_counter) % 50) == 1) {
    //   HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    // }

    // if ((led_toggle_counter % 100) == 1) {
    //   RNG_test();
    // }

    // if ((led_toggle_counter % 200) == 1) {
    //     log_debug("read_adc: %u\n\r", read_adc());
    // }

    HAL_IWDG_Refresh(&hiwdg); // feed wdt
  }
}
