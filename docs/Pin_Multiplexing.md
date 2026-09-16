# Hardware Resource Multiplexing (Pin Sharing)

## Overview

This document explains the mechanism implemented to share physical GPIO pins between the `eink-driver` and the `external_flash` components. To minimize pin count on the ESP32-C6, two pins are shared:

1.  **DC (Data/Command) Pin**: 
    *   Used as a **GPIO Output** for the E-Ink display to signal command vs. data mode.
    *   Used as the **MISO (Master In Slave Out)** signal for the External Flash SPI bus.
2.  **MISO Pin**:
    *   In this specific configuration, the MISO pin is shared/reconfigured to allow the SPI peripheral to communicate with the flash while the E-Ink driver is inactive.

## Mechanism: The "Resource Guard" Approach

Instead of manual pin reconfiguration in every driver, we use a **state-aware hardware lock** (`hardware_lock`) that acts as a resource guard. 

### 1. Context-Aware Locking
The `hardware_lock_acquire()` function has been updated to accept a `hw_state_t` parameter. This allows the lock to know *which* component is requesting access.

```c
// Example usage in eink-driver.c
hardware_lock_acquire(HW_STATE_EINK);

// Example usage in external_flash_manager.c
hardware_lock_acquire(HW_STATE_FLASH);
```

### 2. Automatic Pin Reconfiguration
When a lock is acquired, the `hardware_lock` automatically performs the necessary low-level ESP-IDF/ROM calls to reconfigure the shared pins:

#### Transition to E-Ink Mode (`HW_STATE_EINK`)
*   **Reset Pin**: Returns the DC pin to standard GPIO mode using `gpio_reset_pin()`.
*   **Direction**: Sets the DC pin as `GPIO_MODE_OUTPUT`.
*   **Initial State**: Sets the DC pin high to ensure a known state.

#### Transition to Flash Mode (`HW_STATE_FLASH`)
*   **Direction**: Sets the DC pin as `GPIO_MODE_INPUT`. This is critical; without switching from Output to Input, the SPI controller cannot read the MISO signal.
*   **Routing**: Uses the ESP-IDF ROM API `esp_rom_gpio_connect_in_signal()` to route the internal SPI MISO signal (`FSPIQ_IN_IDX`) to the physical GPIO pin.

## Implementation Details

| Feature | E-Ink Mode | Flash Mode |
| :--- | :--- | :--- |
| **Shared Pin Function** | GPIO Output (DC) | SPI MISO (Input) |
| **API Used** | `gpio_set_direction` | `esp_rom_gpio_connect_in_signal` |
| **Signal Index** | N/A | `FSPIQ_IN_IDX` |

## Assumptions & Constraints

1.  **ESP-IDF Version**: This implementation targets **ESP-IDF 6.1** and utilizes the `esp_rom_gpio.h` headers.
2.  **Hardware Platform**: Specifically designed for the **ESP32-C6** SoC, using its specific signal mapping (`soc/gpio_sig_map.h`).
3.  **Exclusive Access**: It is assumed that all components sharing these pins *must* use the `hardware_lock` to acquire access. Bypassing the lock will result in pin contention and potential hardware/filesystem corruption.
4.  **Write-Only E-Ink**: The `eink-driver` is assumed to be a write-only SPI device (MOSI only), which is why it can safely share the MISO line without requiring a return path for its own SPI transactions.
5.  **Atomic Transitions**: The reconfiguration happens inside the critical section of the recursive mutex, ensuring no other task can attempt to use the pins while they are in an intermediate state.
