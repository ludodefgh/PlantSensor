/*
 * Auto-test des 29 GPIO exposés — voir selftest.h et
 * NRF54L_breakout/test-fixture/TEST-FLOW.md pour le protocole complet.
 *
 * Résumé du raisonnement (détaillé dans TEST-FLOW.md) :
 *
 *   Passe A (pull-up interne) + Passe B (pull-down interne), 100% passives
 *   (jamais de driver de sortie engagé) donnent, par pin :
 *
 *     lu(A)  lu(B)  diagnostic
 *      0      0     sain — ou court vers GND adjacent (indiscernable ici)
 *      1      0     coupure (joint module/header ouvert)
 *      1      1     court vers VDD_NRF
 *      0      1     anomalie (ne devrait pas arriver électriquement)
 *
 *   Passe C (walking-high active, drive standard S0S1, impulsion courte)
 *   ne pilote QUE les pins encore "sain-en-attente" après A/B — jamais un
 *   pin déjà signalé en défaut. Elle lève l'ambiguïté restante (court GND
 *   adjacent, pont entre deux GPIO) sans jamais driver un pin dont on sait
 *   déjà qu'il a un problème.
 */

#include "selftest.h"

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/gpio/nordic-nrf-gpio.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <hal/nrf_nfct.h>

LOG_MODULE_REGISTER(selftest, LOG_LEVEL_INF);

/* Durée du pull-up/pull-down interne avant lecture, et de l'impulsion
 * active — largement au-dessus de la constante de temps RC du fixture
 * (1kΩ + quelques pF), mais courte pour minimiser le temps d'exposition
 * d'un pin actif à un défaut non détecté (voir TEST-FLOW.md §Sécurité). */
#define SETTLE_US 500

const struct test_pin selftest_pins[SELFTEST_PIN_COUNT] = {
	/* ── Réseau A — J1 (GPIO_L), 13/13 pattes ─────────────────────── */
	{1,  9, "P1.09",           "J1.2",  2},
	{1, 10, "P1.10",           "J1.3",  3},
	{1, 11, "P1.11",           "J1.4",  4},
	{1, 12, "P1.12",           "J1.5",  5},
	{1, 13, "P1.13",           "J1.6",  6},
	{1, 14, "P1.14",           "J1.7",  7},
	{1,  2, "P1.02 (NFC1)",    "J1.10", 15},
	{1,  3, "P1.03 (NFC2)",    "J1.11", 16},
	{1,  4, "P1.04",           "J1.12", 17},
	{1,  5, "P1.05",           "J1.13", 18},
	{1,  6, "P1.06",           "J1.14", 19},
	{1,  7, "P1.07",           "J1.15", 20},
	{1,  8, "P1.08",           "J1.16", 21},
	/* ── Réseau B — J4 (GPIO_R), pattes 1-13 ──────────────────────── */
	{0,  4, "P0.04",           "J4.2",  40},
	{0,  3, "P0.03",           "J4.3",  38},
	{0,  2, "P0.02",           "J4.4",  41},
	{0,  1, "P0.01",           "J4.5",  35},
	{0,  0, "P0.00",           "J4.6",  34},
	{2, 10, "P2.10",           "J4.7",  33},
	{2,  9, "P2.09",           "J4.8",  32},
	{2,  8, "P2.08",           "J4.9",  31},
	{2,  7, "P2.07",           "J4.10", 30},
	{2,  6, "P2.06",           "J4.11", 29},
	{2,  5, "P2.05",           "J4.12", 28},
	{2,  4, "P2.04",           "J4.13", 27},
	{2,  3, "P2.03",           "J4.14", 25},
	/* ── Réseau C — J4 (GPIO_R), pattes 1-3 (sur 13) ──────────────── */
	{2,  2, "P2.02",           "J4.15", 24},
	{2,  1, "P2.01",           "J4.16", 23},
	{2,  0, "P2.00",           "J4.17", 22},
};

static const struct device *port_dev[3];

static int8_t shorted_to[SELFTEST_PIN_COUNT];   /* index de l'autre pin, -1 sinon */
static enum pin_status status[SELFTEST_PIN_COUNT];

static const struct device *dev_for(uint8_t port)
{
	return port_dev[port];
}

static int read_pin(int i)
{
	return gpio_pin_get_raw(dev_for(selftest_pins[i].port), selftest_pins[i].pin);
}

/* ── Passe A/B : configure tous les pins en entrée avec le pull demandé,
 * laisse le réseau se stabiliser, lit tout. ─────────────────────────── */
static void passive_pass(bool pull_up, uint32_t *bits_out)
{
	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		gpio_flags_t flags = GPIO_INPUT | (pull_up ? GPIO_PULL_UP : GPIO_PULL_DOWN);

		gpio_pin_configure(dev_for(selftest_pins[i].port), selftest_pins[i].pin, flags);
	}

	k_busy_wait(SETTLE_US);

	*bits_out = 0;
	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		int v = read_pin(i);

		if (v < 0) {
			LOG_ERR("Lecture impossible sur %s (%s) : %d",
				selftest_pins[i].header, selftest_pins[i].net, v);
			continue;
		}
		if (v) {
			*bits_out |= BIT(i);
		}
	}
}

/* ── Passe C : walking-high durci, uniquement sur les pins encore
 * PIN_STATUS_PENDING après A/B. Drive strength standard (S0S1, la plus
 * faible disponible) explicitement demandée — jamais high/extra drive. ── */
static void active_pass(void)
{
	/* Tous les pins "pending" en entrée pull-down par défaut pendant la
	 * passe — état de repos attendu pour un pin sain non piloté. */
	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		if (status[i] == PIN_STATUS_PENDING) {
			gpio_pin_configure(dev_for(selftest_pins[i].port), selftest_pins[i].pin,
					    GPIO_INPUT | GPIO_PULL_DOWN);
		}
	}
	k_busy_wait(SETTLE_US);

	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		if (status[i] != PIN_STATUS_PENDING) {
			continue; /* jamais piloter un pin déjà signalé en défaut */
		}

		const struct device *dev = dev_for(selftest_pins[i].port);
		uint8_t pin = selftest_pins[i].pin;

		gpio_pin_configure(dev, pin, GPIO_OUTPUT_ACTIVE | NRF_GPIO_DRIVE_S0S1);
		k_busy_wait(SETTLE_US);

		if (read_pin(i) != 1) {
			/* Commandé haut, relu bas : quelque chose tire ce pin
			 * vers GND plus fort que le driver standard. */
			status[i] = PIN_STATUS_GND_SHORT;
		} else {
			for (int j = 0; j < SELFTEST_PIN_COUNT; j++) {
				if (j == i || status[j] != PIN_STATUS_PENDING) {
					continue;
				}
				if (read_pin(j) == 1) {
					status[j] = PIN_STATUS_INTER_SHORT;
					shorted_to[j] = i;
					status[i] = PIN_STATUS_INTER_SHORT;
					shorted_to[i] = j;
				}
			}
		}

		/* Relâche avant de passer au suivant. */
		gpio_pin_configure(dev, pin, GPIO_INPUT | GPIO_PULL_DOWN);
		k_busy_wait(SETTLE_US);
	}

	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		if (status[i] == PIN_STATUS_PENDING) {
			status[i] = PIN_STATUS_OK;
		}
	}
}

static const char *status_str(enum pin_status s)
{
	switch (s) {
	case PIN_STATUS_OK:          return "OK";
	case PIN_STATUS_OPEN:        return "COUPURE";
	case PIN_STATUS_VDD_SHORT:   return "COURT->VDD";
	case PIN_STATUS_GND_SHORT:   return "COURT->GND";
	case PIN_STATUS_INTER_SHORT: return "PONT";
	case PIN_STATUS_ANOMALY:     return "ANOMALIE";
	default:                     return "?";
	}
}

int selftest_run(void)
{
	static const struct device *const devs[3] = {
		DEVICE_DT_GET(DT_NODELABEL(gpio0)),
		DEVICE_DT_GET(DT_NODELABEL(gpio1)),
		DEVICE_DT_GET(DT_NODELABEL(gpio2)),
	};

	for (int p = 0; p < 3; p++) {
		port_dev[p] = devs[p];
		if (!device_is_ready(port_dev[p])) {
			LOG_ERR("gpio%d non pret", p);
			return -1;
		}
	}

	/* P1.02/P1.03 (J1.10/J1.11) sont en mode antenne NFC par defaut au
	 * reset (datasheet §8.8.1) — pas de Kconfig/DT pour ca sur nRF54L
	 * dans cette version de NCS, donc appel HAL direct. Note : ce
	 * registre PADCONFIG n'est PAS un registre UICR (contrairement au
	 * nRF52) — le changement n'est donc pas persistant en Flash, un
	 * simple appel au boot suffit, pas d'operation "definitive". */
	nrf_nfct_pad_config_enable_set(NRF_NFCT, false);

	uint32_t bits_pu, bits_pd;

	LOG_INF("Passe A (pull-up interne)...");
	passive_pass(true, &bits_pu);

	LOG_INF("Passe B (pull-down interne)...");
	passive_pass(false, &bits_pd);

	int n_pending = 0;

	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		bool a = bits_pu & BIT(i);
		bool b = bits_pd & BIT(i);

		shorted_to[i] = -1;
		if (!a && !b) {
			status[i] = PIN_STATUS_PENDING; /* sain, ou court-GND -> tranché en passe C */
			n_pending++;
		} else if (a && !b) {
			status[i] = PIN_STATUS_OPEN;
		} else if (a && b) {
			status[i] = PIN_STATUS_VDD_SHORT;
		} else {
			status[i] = PIN_STATUS_ANOMALY;
		}
	}

	LOG_INF("Passe C (walking-high durci, %d/%d pins concernes)...",
		n_pending, SELFTEST_PIN_COUNT);
	active_pass();

	int n_fail = 0;

	printk("\n%-14s %-8s %-4s %-11s %s\n", "Net", "Header", "Pad", "Statut", "Detail");
	printk("--------------------------------------------------------------\n");
	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		char detail[24] = "";

		if (status[i] == PIN_STATUS_INTER_SHORT && shorted_to[i] >= 0) {
			snprintk(detail, sizeof(detail), "-> %s",
				 selftest_pins[shorted_to[i]].header);
		}
		if (status[i] != PIN_STATUS_OK) {
			n_fail++;
		}
		printk("%-14s %-8s %-4u %-11s %s\n",
		       selftest_pins[i].net, selftest_pins[i].header, selftest_pins[i].pad,
		       status_str(status[i]), detail);
	}
	printk("--------------------------------------------------------------\n");
	printk("%d/%d OK\n\n", SELFTEST_PIN_COUNT - n_fail, SELFTEST_PIN_COUNT);

	if (n_fail == 0) {
		LOG_INF("Tous les pins testes sont OK.");
	} else {
		LOG_WRN("%d pin(s) en defaut — voir tableau ci-dessus et TEST-FLOW.md.", n_fail);
	}

	/* Remet tout en entree pull-down au repos (etat sur securise). */
	for (int i = 0; i < SELFTEST_PIN_COUNT; i++) {
		gpio_pin_configure(dev_for(selftest_pins[i].port), selftest_pins[i].pin,
				    GPIO_INPUT | GPIO_PULL_DOWN);
	}

	return n_fail;
}

static int cmd_selftest(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int n_fail = selftest_run();

	if (n_fail < 0) {
		shell_error(sh, "Echec init GPIO — voir logs.");
		return -1;
	}
	shell_print(sh, "%d pin(s) en defaut sur %d.", n_fail, SELFTEST_PIN_COUNT);
	return 0;
}

SHELL_CMD_REGISTER(selftest, NULL,
		    "Balaie les 29 GPIO du fixture pull-down (voir test-fixture/TEST-FLOW.md)",
		    cmd_selftest);
