package com.myco.companion

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    MycoConfigScreen()
                }
            }
        }
    }
}

private val BLE_PERMISSIONS = arrayOf(
    Manifest.permission.BLUETOOTH_SCAN,
    Manifest.permission.BLUETOOTH_CONNECT,
)

private fun hasBlePermissions(context: Context): Boolean =
    BLE_PERMISSIONS.all {
        ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
    }

@Composable
fun MycoConfigScreen() {
    val context = LocalContext.current
    val manager = remember { MycoBleManager(context.applicationContext) }
    val connectionState by manager.connectionState.collectAsState()
    val deviceConfig by manager.config.collectAsState()

    var soilDryText by remember { mutableStateOf("") }
    var soilWetText by remember { mutableStateOf("") }
    var intervalText by remember { mutableStateOf("") }
    var targetMode by remember { mutableStateOf(MycoGatt.MODE_BLE) }

    // Pre-remplit les champs avec les valeurs lues sur l'appareil des
    // qu'elles arrivent.
    LaunchedEffect(connectionState) {
        if (connectionState is ConnectionState.Ready) {
            soilDryText = deviceConfig.soilDry.toString()
            soilWetText = deviceConfig.soilWet.toString()
            intervalText = deviceConfig.intervalS.toString()
            targetMode = deviceConfig.targetMode
        }
    }

    var hasPermissions by remember { mutableStateOf(hasBlePermissions(context)) }
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { results -> hasPermissions = results.values.all { it } }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text("Myco Config", style = MaterialTheme.typography.headlineMedium)

        if (!hasPermissions) {
            Text("Cette appli a besoin des permissions Bluetooth pour trouver et configurer le capteur.")
            Button(onClick = { permissionLauncher.launch(BLE_PERMISSIONS) }) {
                Text("Autoriser le Bluetooth")
            }
            return@Column
        }

        StatusLine(connectionState)

        when (connectionState) {
            is ConnectionState.Disconnected, is ConnectionState.Error -> {
                Text(
                    "Passe le capteur en mode config (bouton, ou commande shell " +
                        "'protocol set config' puis 'protocol reboot'), puis cherche-le ici.",
                )
                Button(onClick = { manager.startScan() }) {
                    Text("Rechercher le capteur")
                }
            }

            is ConnectionState.Scanning,
            is ConnectionState.Connecting,
            is ConnectionState.DiscoveringServices,
            -> {
                CircularProgressIndicator()
            }

            is ConnectionState.Ready -> {
                OutlinedTextField(
                    value = soilDryText,
                    onValueChange = { soilDryText = it },
                    label = { Text("Sol sec (valeur ADC brute)") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = soilWetText,
                    onValueChange = { soilWetText = it },
                    label = { Text("Sol humide (valeur ADC brute)") },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = intervalText,
                    onValueChange = { intervalText = it },
                    label = { Text("Intervalle de report (secondes)") },
                    modifier = Modifier.fillMaxWidth(),
                )

                Text("Mode radio au redemarrage :")
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(
                        selected = targetMode == MycoGatt.MODE_BLE,
                        onClick = { targetMode = MycoGatt.MODE_BLE },
                        label = { Text("BLE") },
                    )
                    FilterChip(
                        selected = targetMode == MycoGatt.MODE_ZIGBEE,
                        onClick = { targetMode = MycoGatt.MODE_ZIGBEE },
                        label = { Text("Zigbee") },
                    )
                }

                Button(
                    onClick = {
                        manager.applyAndReboot(
                            MycoConfigState(
                                soilDry = soilDryText.toIntOrNull() ?: deviceConfig.soilDry,
                                soilWet = soilWetText.toIntOrNull() ?: deviceConfig.soilWet,
                                intervalS = intervalText.toIntOrNull() ?: deviceConfig.intervalS,
                                targetMode = targetMode,
                            ),
                        )
                    },
                ) {
                    Text("Appliquer et redemarrer")
                }
            }

            is ConnectionState.AppliedRebooting -> {
                val modeLabel = if (targetMode == MycoGatt.MODE_ZIGBEE) "Zigbee" else "BLE"
                Text("Configuration appliquee — le capteur redemarre en mode $modeLabel.")
                Button(onClick = { manager.reset() }) {
                    Text("Fermer")
                }
            }
        }
    }
}

@Composable
private fun StatusLine(state: ConnectionState) {
    val text = when (state) {
        is ConnectionState.Disconnected -> "Non connecte"
        is ConnectionState.Scanning -> "Recherche du capteur..."
        is ConnectionState.Connecting -> "Connexion..."
        is ConnectionState.DiscoveringServices -> "Lecture de la configuration..."
        is ConnectionState.Ready -> "Connecte"
        is ConnectionState.AppliedRebooting -> "Redemarrage en cours"
        is ConnectionState.Error -> "Erreur : ${state.message}"
    }
    Text(text, style = MaterialTheme.typography.bodyMedium)
}
