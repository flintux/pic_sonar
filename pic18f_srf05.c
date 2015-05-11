/* 
 * File:   pic18f_srf05.c
 * Author: raraujo
 *
 * Created on 4. mai 2015, 20:02
 */

#include <htc.h>

/**
 * Bits de configuration:
 */
#pragma config FOSC = INTIO67  // Oscillateur interne, ports A6 et A7 comme IO.
#pragma config IESO = OFF      // Pas d'embrouilles avec l'osc. au démarrage.
#pragma config FCMEN = OFF     // Pas de monitorage de l'oscillateur.

// Nécessaires pour ICSP / ICD:
#pragma config MCLRE = EXTMCLR // RE3 est actif comme master reset.
#pragma config WDTEN = OFF     // Watchdog inactif (pour ICSP /ICD)
#pragma config LVP = OFF       // Single Supply Enable bits off.


typedef enum {
    REPOS,
    ATTENTE,
    MESURE
} etat_t;

typedef enum {
    TICTAC,
    DEMARRE,
    ECHO_UP,
    ECHO_DOWN
}event_t;

int derniereMesure = 0;

int machine_sonar(event_t event);

void maj_derniere_mesure(int mesure);
int lire_derniere_mesure(void);

void maj_derniere_mesure(int mesure)
{
    derniereMesure = mesure;
}

int lire_derniere_mesure(void)
{
    return derniereMesure;
}

void low_priority interrupt interruptionsBP() {
    // interrupt TMR4 (TIC TAC)
    if (PIR5bits.TMR4IF) {
        PIR5bits.TMR4IF = 0;
        if (machine_sonar(TICTAC) < 0) {
            LATAbits.LA5 = 0b1;
        } else
        {
            LATAbits.LA5 = 0b0;
            machine_sonar(DEMARRE);
        }
    }

    // interrupt INT2 ECHO
    if (INTCON3bits.INT2IF) {
        // clear IRQ
        INTCON3bits.INT2IF = 0;

        // si IRQ flanc montant
        if (INTCON2bits.INTEDG2 == 1) {
            // mettre l'IRQ au flanc descendant
            INTCON2bits.INTEDG2 = 0;

            machine_sonar(ECHO_UP);


        // si IRQ flanc descendant
        } else {
            // mettre l'IRQ au flanc montant
            INTCON2bits.INTEDG2 = 1;

            machine_sonar(ECHO_DOWN);
        }
    }

}

/*
 *
 */
int machine_sonar(event_t event)
{

    static etat_t status = REPOS;
    static int cptTicTac = -2;

    switch (status) {
        case REPOS:
            switch (event) {
                case TICTAC:
                    cptTicTac++;
                    break;
                case DEMARRE:
                    cptTicTac = 0;
                    status = ATTENTE;
                    break;
                case ECHO_UP:
                    break;
                case ECHO_DOWN:
                    break;
            }
            break;
        case ATTENTE:
            switch (event) {
                case TICTAC:
                    cptTicTac++;
                    if (cptTicTac > 15)
                    {
                        // on a trop attendu, pulse invalide
                        cptTicTac = -2;
                        status = REPOS;
                    }
                    break;
                case DEMARRE:
                    break;
                case ECHO_UP:
                    cptTicTac = 0;
                    status = MESURE;
                    break;
                case ECHO_DOWN:
                    break;
            }
            break;
        case MESURE:
            switch (event) {
                case TICTAC:
                    cptTicTac++;
                    if (cptTicTac > 600)
                    {
                        // mesure trop long, petit problem
                        status = REPOS;
                        cptTicTac = -2;
                    }
                    break;
                case DEMARRE:
                    break;
                case ECHO_UP:
                    cptTicTac = 0;
                    break;
                case ECHO_DOWN:
                    maj_derniere_mesure(cptTicTac);
                    cptTicTac = -2;
                    status = REPOS;
                    break;
            }
            break;
    }
    return cptTicTac;
}

/*
 * 
 */
void main(void) {

    ANSELA = 0x00; // Désactive les convertisseurs A/D.
    ANSELB = 0x00; // Désactive les convertisseurs A/D.
    ANSELC = 0x00; // Désactive les convertisseurs A/D.


    // configuration de l'horloge
    OSCCONbits.IRCF = 0b111;    // Frequence de base: 16 MHz
    OSCTUNEbits.PLLEN = 0b1;      // utilise la PLL (horloge x 4)
    OSCCONbits.SCS = 0b00;      // utilise l'oscillateur primaire
    

    //config du TMR4 pour 58uS par OF
    T4CONbits.T4OUTPS = 0b0000;     // pas de div entre TMR et IRQ
    T4CONbits.T4CKPS = 0b01;        // diviser par 4, FOSC/4 et entré TMR
    PR4 = 232;                      // 232 cycles * 250ns = 58us
    T4CONbits.TMR4ON = 0b1;         // activer le timer

    // config interrupt TMR4
    IPR5bits.TMR4IP = 0;            // interrupt basse prio
    PIE5bits.TMR4IE = 1;            // activer interrupt TMR4

    // config pour trigger
    TRISAbits.RA5 = 0b0;        // RA5 sortie trigger

    // config echo
    INTCON2bits.RBPU = 0;       // activer les résistances de tirage
    TRISBbits.RB2 = 0b1;        // RB2 entrée
    WPUBbits.WPUB2 = 0b1;       // activer pull up RB2


    INTCON3bits.INT2IP = 0;     // INT2 basse priotité
    INTCON2bits.INTEDG2 = 1;        // flanc montant
    INTCON3bits.INT2IE = 1;         // activer INT2


    // activer les interruptions
    RCONbits.IPEN = 1;          // activer les niveau d'interrupt
    // interruption haute priorité
    INTCONbits.GIEH = 0;
    // interruptions basse priorité
    INTCONbits.GIEL = 1;

    while(1);

}

