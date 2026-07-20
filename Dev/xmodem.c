/**
 * @file    xmodem.c
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module is the implementation of the Xmodem protocol.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#include "xmodem.h"
#include "logging.h"
#include "usart.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Global variables. */
static uint8_t xmodem_packet_number = 1u;         /**< Packet number counter. */
static uint32_t xmodem_actual_flash_address = 0u; /**< Address where we have to write. */
static uint8_t x_first_packet_received = false;   /**< First packet or not. */

/* Local functions. */
static uint16_t xmodem_calc_crc(uint8_t *data, uint16_t length);
static xmodem_status xmodem_handle_packet(uint8_t size);
static xmodem_status xmodem_error_handler(uint8_t *error_number, uint8_t max_error_number);

/*necessary wrappers*/
#define FLASH_OK 0
#define FLASH_APP_START_ADDRESS 0x8020000 // 128kb

typedef enum {
  UART_OK = 0,
  UART_NOK = 1
} uart_status;
uint8_t flash_erase(uint32_t start_address);
uint8_t flash_write(uint32_t address, uint32_t *buffer, uint32_t len);
uart_status uart_receive(uint8_t *buffer, int32_t len);
uart_status uart_transmit_ch(uint8_t chchar);

uint8_t flash_erase(uint32_t start_address) {
  log_debug("TODO: delete flash from starting address 0x%lX\n\r", start_address);
  return FLASH_OK;
}

uint8_t flash_write(uint32_t address, uint32_t *buffer, uint32_t len) {
  log_debug("Flash write to 0x%08lX (%lu words):\n\r", address, len);
  for (uint32_t i = 0; i < len; i++) {
    log_debug(" 0x%08lX", buffer[i]);  // ✅ Düzeltildi: 0x.08lX → 0x%08lX
    if ((i + 1) % 8 == 0) {
      log_debug("\n\r");  // Her 8 word'de satır atla
    }
  }
  if (len % 8 != 0) {
    log_debug("\n\r");
  }
  return FLASH_OK;
}

extern uint8_t xmodem_buffer[2048];
extern uint32_t xmodem_buffer_counter;
static uint32_t xmodem_buffer_index = 0;
uart_status uart_receive(uint8_t *buffer, int32_t len) {
//   // ✅ RX buffer'ı temizle (eski data kalmasın)
//   __HAL_UART_FLUSH_DRREGISTER(&huart3);
  
  HAL_StatusTypeDef status = HAL_UART_Receive(&huart3, buffer, len, 5000);
  
  if (status == HAL_OK) {
    return UART_OK;
  }
  
  return UART_NOK;
}

uart_status uart_transmit_ch(uint8_t ch) {
  uint8_t localbuff[1] = {ch};
  
  // ✅ UART3 aktif mi kontrol et
  if (huart3.Instance == NULL) {
    log_debug("ERROR: UART3 not initialized!\n\r");
    return UART_NOK;
  }
  
  // ✅ Gönderilecek byte'ı logla
  log_debug(">>> TX: 0x%02X ('%c') <<<\n\r", ch, 
            (ch >= 32 && ch < 127) ? ch : '.');
  
  HAL_StatusTypeDef status = HAL_UART_Transmit(&huart3, localbuff, 1, 1000);
  
  if (status != HAL_OK) {
    log_debug("ERROR: HAL_UART_Transmit failed! Status: %d\n\r", status);
    return UART_NOK;
  }
  
  log_debug(">>> TX OK <<<\n\r");
  return UART_OK;
}

/**
 * @brief   This function is the base of the Xmodem protocol.
 *          When we receive a header from UART, it decides what action it shall take.
 * @param   void
 * @return  void
 */
void xmodem_receive(void)
{
  volatile xmodem_status status = X_OK;
  uint8_t error_number = 0u;

  x_first_packet_received = false;
  xmodem_packet_number = 1u;
  xmodem_actual_flash_address = FLASH_APP_START_ADDRESS;

  log_debug("=== Xmodem Receiver Started ===\n\r");

  // ✅ İlk 'C' gönder (CRC16 mode)
  uart_transmit_ch(X_C);
  log_debug("Sent initial 'C'\n\r");

    uint32_t total_packets = 0;

  while (X_OK == status)
  {
    uint8_t header = 0x00u;
    uart_status comm_status = uart_receive(&header, 1u);

    // ✅ Timeout handling
    if ((UART_OK != comm_status) && (false == x_first_packet_received))
    {
      // İlk paket gelmedi, 'C' göndermeye devam et
      error_number++;
      if (error_number >= X_MAX_ERRORS)
      {
        log_debug("ERROR: Timeout waiting for first packet\n\r");
        status = X_ERROR;
        break;
      }
      
      uart_transmit_ch(X_C);
      log_debug("Timeout - resending 'C' (%d/%d)\n\r", error_number, X_MAX_ERRORS);
      continue;
    }
    else if ((UART_OK != comm_status) && (true == x_first_packet_received))
    {
      log_debug("ERROR: UART timeout after first packet\n\r");
      status = xmodem_error_handler(&error_number, X_MAX_ERRORS);
      continue;
    }

    // ✅ Header alındı
    log_debug("\n>>> Header: 0x%02X <<<\n", header);

    switch(header)
    {
      case X_SOH:
      case X_STX:
      {
        log_debug("Processing %s packet #%d...\n\r", 
                  (header == X_SOH) ? "SOH(128)" : "STX(1024)", 
                  xmodem_packet_number);
        
        xmodem_status packet_status = xmodem_handle_packet(header);
        
        if (X_OK == packet_status)
        {
          // ✅ BAŞARILI - ACK GÖNDER
          uart_transmit_ch(X_ACK);
            total_packets++;
            
            // Her 100 pakette bir ilerleme göster
            if (total_packets % 100 == 0) {
                log_debug("Progress: %lu packets\n\r", 
                        total_packets);
            }
          log_debug(">>> ACK sent for packet %d <<<\n\r", xmodem_packet_number - 1);
          error_number = 0;  // Hata sayacını sıfırla
        }
        else if (X_ERROR_FLASH == packet_status)
        {
          log_debug("ERROR: Flash error\n\r");
          error_number = X_MAX_ERRORS;
          status = xmodem_error_handler(&error_number, X_MAX_ERRORS);
        }
        else
        {
          log_debug("ERROR: Packet failed (0x%02X)\n\r", packet_status);
          status = xmodem_error_handler(&error_number, X_MAX_ERRORS);
        }
        break;
      }
        
      case X_EOT:
        log_debug("=== EOT received ===\n\r");
        uart_transmit_ch(X_ACK);
        log_debug("✓ Transfer complete!\n\r");
        status = X_ERROR;  // Loop'tan çık
        break;
        
      case X_CAN:
        log_debug("✗ CAN received - transfer cancelled\n\r");
        status = X_ERROR;
        break;
        
      default:
        log_debug("ERROR: Invalid header 0x%02X\n\r", header);
        status = xmodem_error_handler(&error_number, X_MAX_ERRORS);
        break;
    }
  }
  
  log_debug("=== Xmodem ended (status: %d) ===\n\r", status);
    log_debug("=== Transfer complete ===\n\r");
  log_debug("Total: %lu packets\n\r",
            total_packets);
}

/**
 * @brief   Calculates the CRC-16 for the input package.
 * @param   *data:  Array of the data which we want to calculate.
 * @param   length: Size of the data, either 128 or 1024 bytes.
 * @return  status: The calculated CRC.
 */
static uint16_t xmodem_calc_crc(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0u;
    while (length)
    {
        length--;
        crc = crc ^ ((uint16_t)*data++ << 8u);
        for (uint8_t i = 0u; i < 8u; i++)
        {
            if (crc & 0x8000u)
            {
                crc = (crc << 1u) ^ 0x1021u;
            }
            else
            {
                crc = crc << 1u;
            }
        }
    }
    return crc;
}

/**
 * @brief   This function handles the data packet we get from the xmodem protocol.
 * @param   header: SOH or STX.
 * @return  status: Report about the packet.
 */
static xmodem_status xmodem_handle_packet(uint8_t header)
{
  xmodem_status status = X_OK;
  uint16_t size = 0u;

  uint8_t received_packet_number[X_PACKET_NUMBER_SIZE];
  uint8_t received_packet_data[X_PACKET_1024_SIZE];
  uint8_t received_packet_crc[X_PACKET_CRC_SIZE];

  // Paket boyutu belirle
  if (X_SOH == header)
    size = X_PACKET_128_SIZE;
  else if (X_STX == header)
    size = X_PACKET_1024_SIZE;
  else
    return X_ERROR;

  // Tüm paketi al
  uart_status comm_status = UART_OK;
  comm_status |= uart_receive(&received_packet_number[0u], X_PACKET_NUMBER_SIZE);
  comm_status |= uart_receive(&received_packet_data[0u], size);
  comm_status |= uart_receive(&received_packet_crc[0u], X_PACKET_CRC_SIZE);

  if (UART_OK != comm_status)
  {
    log_debug("ERROR: UART receive failed\n\r");
    return X_ERROR_UART;
  }

  uint8_t received_pkt_num = received_packet_number[X_PACKET_NUMBER_INDEX];
  
  // ✅ KRİTİK: DUPLICATE PAKET KONTROLÜ
  // Sender ACK görmemiş ve önceki paketi tekrar göndermiş
  if (received_pkt_num == (uint8_t)(xmodem_packet_number - 1))
  {
    log_debug("⚠️  DUPLICATE: Packet %d (sender didn't get ACK)\n\r", received_pkt_num);
    // Flash'a tekrar yazma, sadece ACK gönder
    return X_OK;
  }
  
  // ✅ Beklenen paket mi?
  if (received_pkt_num != xmodem_packet_number)
  {
    log_debug("ERROR: Expected pkt %d, got %d\n\r", xmodem_packet_number, received_pkt_num);
    return X_ERROR_NUMBER;
  }

  // ✅ Complement kontrolü
  if (255u != (received_packet_number[X_PACKET_NUMBER_INDEX] + 
               received_packet_number[X_PACKET_NUMBER_COMPLEMENT_INDEX]))
  {
    log_debug("ERROR: Complement check failed (%d + %d != 255)\n\r",
              received_packet_number[X_PACKET_NUMBER_INDEX],
              received_packet_number[X_PACKET_NUMBER_COMPLEMENT_INDEX]);
    return X_ERROR_NUMBER;
  }

  // ✅ CRC kontrolü
  uint16_t crc_received = ((uint16_t)received_packet_crc[X_PACKET_CRC_HIGH_INDEX] << 8u) | 
                          ((uint16_t)received_packet_crc[X_PACKET_CRC_LOW_INDEX]);
  uint16_t crc_calculated = xmodem_calc_crc(&received_packet_data[0u], size);

  if (crc_received != crc_calculated)
  {
    log_debug("ERROR: CRC mismatch! Rx:0x%04X Calc:0x%04X\n\r",
              crc_received, crc_calculated);
    return X_ERROR_CRC;
  }

  // ✅ İlk paket için flash sil
  if (false == x_first_packet_received)
  {
    log_debug("First packet - erasing flash from 0x%08lX\n\r", FLASH_APP_START_ADDRESS);
    if (FLASH_OK == flash_erase(FLASH_APP_START_ADDRESS))
    {
      x_first_packet_received = true;
    }
    else
    {
      return X_ERROR_FLASH;
    }
  }

  // ✅ Flash write
  log_debug("Writing to flash: 0x%08lX (%d bytes)\n\r", xmodem_actual_flash_address, size);
  
  if (FLASH_OK != flash_write(xmodem_actual_flash_address, 
                               (uint32_t*)&received_packet_data[0u], 
                               (uint32_t)size/4u))
  {
    log_debug("ERROR: Flash write failed\n\r");
    return X_ERROR_FLASH;
  }

  // ✅ Başarılı - sayaçları artır
  xmodem_packet_number++;
  xmodem_actual_flash_address += size;
  
  log_debug("✓ Pkt %d OK | Next: %d | Total: %lu bytes\n\r", 
            received_pkt_num,
            xmodem_packet_number,
            xmodem_actual_flash_address - FLASH_APP_START_ADDRESS);

  return X_OK;
}

/**
 * @brief   Handles the xmodem error.
 *          Raises the error counter, then if the number of the errors reached critical, do a graceful abort, otherwise send a NAK.
 * @param   *error_number:    Number of current errors (passed as a pointer).
 * @param   max_error_number: Maximal allowed number of errors.
 * @return  status: X_ERROR in case of too many errors, X_OK otherwise.
 */
static xmodem_status xmodem_error_handler(uint8_t *error_number, uint8_t max_error_number)
{
  xmodem_status status = X_OK;
  /* Raise the error counter. */
  (*error_number)++;
  /* If the counter reached the max value, then abort. */
  if ((*error_number) >= max_error_number)
  {
    /* Graceful abort. */
    (void)uart_transmit_ch(X_CAN);
    (void)uart_transmit_ch(X_CAN);
    status = X_ERROR;
  }
  /* Otherwise send a NAK for a repeat. */
  else
  {
    (void)uart_transmit_ch(X_NAK);
    status = X_OK;
  }
  return status;
}
