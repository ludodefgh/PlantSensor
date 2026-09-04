/*
 * NRF54L_breakout — firmware d'auto-test des GPIO exposés.
 *
 * Pas de capteurs/BLE : lance une passe au boot puis attend les commandes
 * du shell RTT (`selftest` pour relancer). Voir
 * NRF54L_breakout/test-fixture/TEST-FLOW.md pour le protocole complet et
 * le câblage du fixture.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "selftest.h"

LOG_MODULE_REGISTER(myco_selftest, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("Myco pin-selftest booting on nRF54L15...");
	LOG_INF("Shell RTT pret — tape 'selftest' pour relancer le balayage.");

	k_msleep(200); /* laisse le fixture (RC 1kOhm) se stabiliser au boot */
	selftest_run();

	return 0;
}
