#include "PicoTransport.h"
#include <Arduino.h>
#include <string.h>
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

static uint8_t rx_buffer[BUFFER_LENGTH];
static volatile uint8_t rx_index = 0;
static volatile bool rx_overflow = false;

static uint8_t ready_buffer[BUFFER_LENGTH];
static volatile bool packet_ready = false;

static inline uint get_spi_irq_num(void) {
    return (spi_get_index(PICO_SPI_SLAVE_INSTANCE) == 0) ? SPI0_IRQ : SPI1_IRQ;
}

static void drain_spi_rx(void) {
    spi_hw_t *hw = spi_get_hw(PICO_SPI_SLAVE_INSTANCE);
    if (hw->ris & SPI_SSPRIS_RORRIS_BITS) {
        rx_overflow = true;
    }

    while (spi_is_readable(PICO_SPI_SLAVE_INSTANCE)) {
        uint8_t byte = (uint8_t)hw->dr;
        if (rx_index < BUFFER_LENGTH) {
            rx_buffer[rx_index++] = byte;
        }
    }

    hw->icr = SPI_SSPICR_RTIC_BITS | SPI_SSPICR_RORIC_BITS;
}

static void on_spi_irq(void) {
    drain_spi_rx();
}

// Interrupt Service Routine triggered on CS edges to frame each 20-byte packet
static void on_cs_edge(uint gpio, uint32_t events) {
    (void)gpio;

    if (events & GPIO_IRQ_EDGE_FALL) {
        // Start of frame: flush any stale pre-frame FIFO bytes and reset index
        spi_hw_t *hw = spi_get_hw(PICO_SPI_SLAVE_INSTANCE);
        while (spi_is_readable(PICO_SPI_SLAVE_INSTANCE)) {
            (void)hw->dr;
        }
        hw->icr = SPI_SSPICR_RTIC_BITS | SPI_SSPICR_RORIC_BITS;
        rx_index = 0;
        rx_overflow = false;
    }

    if (events & GPIO_IRQ_EDGE_RISE) {
        // End of frame: drain remaining FIFO bytes before checking frame length
        drain_spi_rx();
        if (!rx_overflow && rx_index == BUFFER_LENGTH) {
            memcpy(ready_buffer, rx_buffer, BUFFER_LENGTH);
            packet_ready = true;
        }
        rx_index = 0;
        rx_overflow = false;
    }
}

void pico_spi_slave_init(void) {
    // 1. Initialize SPI at up to 2MHz
    spi_init(PICO_SPI_SLAVE_INSTANCE, 2000000);

    // 2. Set SPI format to Mode 3: 8 data bits, CPOL=1, CPHA=1, MSB first
    spi_set_format(PICO_SPI_SLAVE_INSTANCE, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    // 3. Put Pico SPI into SLAVE mode (Slave to CDJ Master Clock)
    spi_set_slave(PICO_SPI_SLAVE_INSTANCE, true);

    // 4. Assign GPIO pins to SPI functions
    gpio_set_function(PICO_SPI_PIN_RX,  GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_PIN_TX,  GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_PIN_CS,  GPIO_FUNC_SPI);

    // Flush any initial FIFO garbage before enabling interrupts
    spi_hw_t *hw = spi_get_hw(PICO_SPI_SLAVE_INSTANCE);
    while (spi_is_readable(PICO_SPI_SLAVE_INSTANCE)) {
        (void)hw->dr;
    }
    hw->icr = SPI_SSPICR_RTIC_BITS | SPI_SSPICR_RORIC_BITS;

    rx_index = 0;
    rx_overflow = false;
    packet_ready = false;

    // 5. Drain RX FIFO via SPI IRQ so 20-byte frames never overflow the 8-entry FIFO
    uint spi_irq = get_spi_irq_num();
    irq_set_exclusive_handler(spi_irq, on_spi_irq);
    hw->imsc = SPI_SSPIMSC_RXIM_BITS | SPI_SSPIMSC_RTIM_BITS | SPI_SSPIMSC_RORIM_BITS;
    irq_set_enabled(spi_irq, true);

    // 6. Monitor Chip Select edges to align and commit 20-byte packet boundaries
    gpio_set_irq_enabled_with_callback(
        PICO_SPI_PIN_CS,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true,
        &on_cs_edge
    );
}

bool pico_spi_slave_read_packet(StatusMessage *out_msg) {
    uint8_t packet[BUFFER_LENGTH];
    bool has_packet = false;

    uint32_t irq_state = save_and_disable_interrupts();
    if (packet_ready) {
        memcpy(packet, ready_buffer, BUFFER_LENGTH);
        packet_ready = false;
        has_packet = true;
    }
    restore_interrupts(irq_state);

    if (has_packet) {
        // --- Professor's instruction: Serial.print raw received data ---
        if (Serial) {
            Serial.print("SPI RX [20B]: ");
            for (size_t i = 0; i < BUFFER_LENGTH; i++) {
                if (packet[i] < 0x10) Serial.print("0");
                Serial.print(packet[i], HEX);
                Serial.print(" ");
            }
            Serial.println();
        }

        // Verify CRC using Alejandro's StatusMessage validator
        if (StatusMessage::isValid(packet)) {
            *out_msg = StatusMessage::createMessage(packet);
            return true;
        } else if (Serial) {
            Serial.println(" -> CRC Mismatch / Frame Ignored");
        }
    }

    return false;
}
