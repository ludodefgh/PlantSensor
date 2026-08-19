package com.myco.companion

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.ParcelUuid
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.util.UUID
import java.util.ArrayDeque

sealed interface ConnectionState {
    data object Disconnected : ConnectionState
    data object Scanning : ConnectionState
    data object Connecting : ConnectionState
    data object DiscoveringServices : ConnectionState
    data object Ready : ConnectionState
    data object AppliedRebooting : ConnectionState
    data class Error(val message: String) : ConnectionState
}

data class MycoConfigState(
    val soilDry: Int = 3500,
    val soilWet: Int = 1260,
    val intervalS: Int = 10,
    val targetMode: Int = MycoGatt.MODE_BLE,
)

/**
 * Client GATT pour le service de config du capteur Myco (mode "config" du
 * firmware, cf. ble_config_app.c). Une seule operation GATT peut etre en
 * vol a la fois sur Android (limite de la plateforme, pas documentee
 * clairement mais tres largement connue) — toute lecture/ecriture passe
 * donc par une file interne traitee une par une.
 */
class MycoBleManager(private val context: Context) {

    private val bluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter get() = bluetoothManager.adapter

    private var gatt: BluetoothGatt? = null
    private val characteristics = mutableMapOf<UUID, BluetoothGattCharacteristic>()

    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _config = MutableStateFlow(MycoConfigState())
    val config: StateFlow<MycoConfigState> = _config.asStateFlow()

    // ── File d'operations GATT (une seule en vol a la fois) ──────────
    private val operationQueue = ArrayDeque<() -> Unit>()
    private var operationInFlight = false

    private fun enqueue(op: () -> Unit) {
        synchronized(operationQueue) {
            operationQueue.addLast(op)
            if (!operationInFlight) {
                operationInFlight = true
                operationQueue.removeFirst().invoke()
            }
        }
    }

    private fun completeOperation() {
        synchronized(operationQueue) {
            val next = operationQueue.poll()
            if (next == null) {
                operationInFlight = false
            } else {
                next.invoke()
            }
        }
    }

    // ── Scan ───────────────────────────────────────────────────────
    @SuppressLint("MissingPermission")
    fun startScan() {
        val a = adapter
        if (a == null || !a.isEnabled) {
            _connectionState.value = ConnectionState.Error("Bluetooth desactive")
            return
        }
        _connectionState.value = ConnectionState.Scanning
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(MycoGatt.SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        a.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            stopScan()
            _connectionState.value = ConnectionState.Connecting
            gatt = result.device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        }

        override fun onScanFailed(errorCode: Int) {
            _connectionState.value = ConnectionState.Error("Scan echoue (code $errorCode)")
        }
    }

    // ── GATT ───────────────────────────────────────────────────────
    private val gattCallback = object : BluetoothGattCallback() {

        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.DiscoveringServices
                    g.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    // Apres "commit", le capteur reboote et coupe la
                    // connexion : c'est le comportement attendu, pas une
                    // erreur — on ne doit pas ecraser l'etat AppliedRebooting.
                    if (_connectionState.value != ConnectionState.AppliedRebooting) {
                        _connectionState.value = ConnectionState.Disconnected
                    }
                    g.close()
                    gatt = null
                    characteristics.clear()
                    operationQueue.clear()
                    operationInFlight = false
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                _connectionState.value = ConnectionState.Error("Decouverte des services echouee")
                return
            }
            val service = g.getService(MycoGatt.SERVICE_UUID)
            if (service == null) {
                _connectionState.value =
                    ConnectionState.Error("Service Myco introuvable (mauvais appareil ?)")
                return
            }
            listOf(
                MycoGatt.SOIL_DRY_UUID, MycoGatt.SOIL_WET_UUID,
                MycoGatt.INTERVAL_UUID, MycoGatt.TARGET_MODE_UUID, MycoGatt.COMMIT_UUID,
            ).forEach { uuid ->
                service.getCharacteristic(uuid)?.let { characteristics[uuid] = it }
            }
            readAllConfig(g)
        }

        @Suppress("DEPRECATION")
        override fun onCharacteristicRead(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val bytes = characteristic.value
                if (bytes != null) {
                    applyReadValue(characteristic.uuid, bytes)
                }
            }
            completeOperation()
        }

        @Suppress("DEPRECATION")
        override fun onCharacteristicWrite(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            completeOperation()
        }
    }

    private fun applyReadValue(uuid: UUID, bytes: ByteArray) {
        when (uuid) {
            MycoGatt.SOIL_DRY_UUID -> _config.update { it.copy(soilDry = readU16(bytes)) }
            MycoGatt.SOIL_WET_UUID -> _config.update { it.copy(soilWet = readU16(bytes)) }
            MycoGatt.INTERVAL_UUID -> _config.update { it.copy(intervalS = readU16(bytes)) }
            MycoGatt.TARGET_MODE_UUID -> _config.update {
                it.copy(targetMode = bytes.getOrNull(0)?.toInt()?.and(0xFF) ?: it.targetMode)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun readAllConfig(g: BluetoothGatt) {
        val uuids = listOf(
            MycoGatt.SOIL_DRY_UUID, MycoGatt.SOIL_WET_UUID,
            MycoGatt.INTERVAL_UUID, MycoGatt.TARGET_MODE_UUID,
        )
        var remaining = uuids.size
        uuids.forEach { uuid ->
            enqueue {
                val ch = characteristics[uuid]
                if (ch == null || !g.readCharacteristic(ch)) {
                    completeOperation()
                }
            }
        }
        // La derniere lecture de la file marque l'etat "Ready" une fois
        // toutes les valeurs recues — on l'accroche via une operation
        // finale plutot que de compter, plus simple a lire.
        enqueue {
            _connectionState.value = ConnectionState.Ready
            completeOperation()
        }
    }

    @SuppressLint("MissingPermission")
    private fun writeRaw(uuid: UUID, bytes: ByteArray, writeType: Int) {
        val g = gatt
        val ch = characteristics[uuid]
        if (g == null || ch == null) {
            completeOperation()
            return
        }
        ch.writeType = writeType
        ch.value = bytes
        @Suppress("DEPRECATION")
        if (!g.writeCharacteristic(ch)) {
            completeOperation()
        }
    }

    /** Ecrit la nouvelle config puis declenche le commit (persiste + reboot
     * cote firmware). La connexion tombera peu apres — c'est le signal de
     * succes, pas une erreur (voir onConnectionStateChange). */
    fun applyAndReboot(newConfig: MycoConfigState) {
        enqueue {
            writeRaw(
                MycoGatt.SOIL_DRY_UUID, u16Bytes(newConfig.soilDry),
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            )
        }
        enqueue {
            writeRaw(
                MycoGatt.SOIL_WET_UUID, u16Bytes(newConfig.soilWet),
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            )
        }
        enqueue {
            writeRaw(
                MycoGatt.INTERVAL_UUID, u16Bytes(newConfig.intervalS),
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            )
        }
        enqueue {
            writeRaw(
                MycoGatt.TARGET_MODE_UUID, byteArrayOf(newConfig.targetMode.toByte()),
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            )
        }
        enqueue {
            _connectionState.value = ConnectionState.AppliedRebooting
            // Le capteur peut redemarrer avant que le callback local
            // n'arrive : on ne bloque pas la file dessus.
            writeRaw(
                MycoGatt.COMMIT_UUID, byteArrayOf(0x01),
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE,
            )
            completeOperation()
        }
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        gatt?.disconnect()
    }

    /** Revient a l'ecran de recherche (apres un succes ou une erreur). */
    fun reset() {
        _connectionState.value = ConnectionState.Disconnected
    }

    private fun u16Bytes(value: Int): ByteArray {
        val v = value.coerceIn(0, 0xFFFF)
        return byteArrayOf((v and 0xFF).toByte(), ((v shr 8) and 0xFF).toByte())
    }

    private fun readU16(bytes: ByteArray): Int {
        if (bytes.size < 2) return 0
        return (bytes[0].toInt() and 0xFF) or ((bytes[1].toInt() and 0xFF) shl 8)
    }
}
