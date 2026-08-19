package com.myco.companion

import java.util.UUID

/**
 * UUIDs et constantes du service GATT de configuration exposé par le
 * firmware en mode "config" (ble_config_app.c, branche
 * feature/an54lq15-dual-protocol). Doit rester en phase avec les
 * BT_UUID_MYCO_CFG_*_VAL définis là-bas.
 */
object MycoGatt {
    val SERVICE_UUID: UUID = UUID.fromString("59ad0000-1212-4a2d-8b1e-2c9f4a5d0001")
    val SOIL_DRY_UUID: UUID = UUID.fromString("59ad0000-1212-4a2d-8b1e-2c9f4a5d0002")
    val SOIL_WET_UUID: UUID = UUID.fromString("59ad0000-1212-4a2d-8b1e-2c9f4a5d0003")
    val INTERVAL_UUID: UUID = UUID.fromString("59ad0000-1212-4a2d-8b1e-2c9f4a5d0004")
    val TARGET_MODE_UUID: UUID = UUID.fromString("59ad0000-1212-4a2d-8b1e-2c9f4a5d0005")
    val COMMIT_UUID: UUID = UUID.fromString("59ad0000-1212-4a2d-8b1e-2c9f4a5d0006")

    const val MODE_BLE: Int = 0
    const val MODE_ZIGBEE: Int = 1
}
