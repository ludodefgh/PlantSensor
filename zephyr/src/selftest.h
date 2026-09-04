/*
 * Auto-test des 29 GPIO exposés du breakout AN54LQ-15, sur le fixture
 * pull-down 1kΩ. Voir NRF54L_breakout/test-fixture/TEST-FLOW.md pour le
 * protocole (3 passes : pull-up, pull-down, walking-high durci) et le
 * raisonnement de sécurité derrière.
 */

#ifndef MYCO_SELFTEST_H
#define MYCO_SELFTEST_H

#include <stdint.h>

#define SELFTEST_PIN_COUNT 29

enum pin_status {
	PIN_STATUS_PENDING = 0,  /* pas encore classé (interne) */
	PIN_STATUS_OK,
	PIN_STATUS_OPEN,         /* joint module/header ouvert */
	PIN_STATUS_VDD_SHORT,    /* court vers VDD_NRF (détecté en passif) */
	PIN_STATUS_GND_SHORT,    /* court vers GND (détecté en actif) */
	PIN_STATUS_INTER_SHORT,  /* pont vers un autre GPIO (détecté en actif) */
	PIN_STATUS_ANOMALY,      /* combinaison pull-up/pull-down inattendue */
};

struct test_pin {
	uint8_t port;        /* 0/1/2 -> gpio0/gpio1/gpio2 */
	uint8_t pin;          /* numéro de pin dans le port */
	const char *net;      /* nom du net, ex "P1.09" */
	const char *header;   /* pin breakout, ex "J1.2" */
	uint8_t pad;           /* numéro de pad module U1 (PIN_MAPPING.md) */
};

/* Table des 29 pins testés — ordre : réseau A (J1, 13 pins) puis
 * réseau B+C (J4, 16 pins), cf. la table de câblage du fixture. */
extern const struct test_pin selftest_pins[SELFTEST_PIN_COUNT];

/* Lance les 3 passes et affiche le tableau de résultats sur le shell/log
 * courant. Retourne le nombre de pins qui ne sont pas PIN_STATUS_OK. */
int selftest_run(void);

#endif /* MYCO_SELFTEST_H */
