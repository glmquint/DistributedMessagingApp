#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "vector.h"


#define TIMEOUT 1              /*tempo che il peer aspetta per ricevere la risposta al boot dal server*/
#define BOOT_MSG 6            /*"10000\0"*/
#define BOOT_RESP 12           /*"10001 10002\0"*/
#define MAX_SIZE_RISULTATO 100 /*definisce la dimesione massima dellarray del risultato delle Aggregazioni*/
#define NUMERO_VICINI 2

#define REQAG_LEN 50 
#define MAX_PEER_DA_CONTATTARE 100

time_t rawtime; /*rawtime di uso globale*/

/*FileManager definitions*/
struct FileManager
{
    char *fileRegistro;
    char *fileArchivio;
} FileManager;

void FileManager_salvaArchivioAggregazioni();
void FileManager_salvaRegistro();
void FileManager_caricaArchivioAggregazioni();
void FileManager_caricaRegistro();





struct sPeer
{
    int requester;
    int sd_flood;
    int richieste_inviate;
    char richiesta_id[20];
    int richiesta_in_gestione; // FIXUP: shouldn't need this if we use another proc
                                // also, seems we need to implement multi request handling
    int risposte_mancanti;
    
    cvector vicini;
    int porta;
    int peer_da_contattare[MAX_PEER_DA_CONTATTARE];
    int numero_peer_da_contattare;
    int ds_port;
    char ds_ip[20];
} sPeer;

/* FIXUP: no need to expose peer methods. Just define 'em in peer.c definition
void sPeer_help();
void sPeer_serverBoot(char *ds_addr, int ds_port);
int sPeer_richiestaAggregazioneNeighbor(char *aggr, char *type, char *period);
void sPeer_gestioneRichiestaAggregazione(int sd);
void sPeer_richiestaRegistri();
void sPeer_stop();
void sPeer_aggiungiPeerDaContattare(int peer);
void sPeer_richiestaFlood(int requester);
void sPeer_invioRispostaFlood();
void sPeer_ricezioneRispostaFlood(int sd);
void sPeer_sendFlood();
int sPeer_verificaRichiestaRicevuta(char richiesta_id[50], int sd);
void sPeer_gestioneFlood(int sd);
void sPeer_menu();
void sPeer_gestioneRichiestaRegistri(int sd);
void IOMultiplex(bool use_udp);
void sPeer_gestisciStop();
void sPeer_disconnessioneNetwork();
void sPeer_gestisciNuoviVicini(int sd);
*/



/*RegistroGenerale definitions*/
struct RegistroGenerale
{
    char *richiesta_aggr, *richiesta_type, *richiesta_period;
    struct tm periodo_inizio, periodo_fine;
    struct DataNecessaria *lista_date_necessarie;
    int numero_date_necessarie;
    struct RegistroGiornaliero *lista_registri_richiesti;
    int numero_registri_richiesti;
    struct RegistroGiornaliero *lista_registri;
    struct RegistroGiornaliero *ultimo_registro;
} RegistroGenerale;

struct RegistroGiornaliero
{
    struct tm data;
    int tamponi;
    int nuovi_casi;
    int completo; /*0 non completo, 1 completo*/
    struct RegistroGiornaliero *next_registro;
};

struct DataNecessaria
{
    struct tm data;
    struct DataNecessaria *next_data;
};

int RegistroGenerale_comparaData(struct tm prima_data, struct tm seconda_data);
void RegistroGenerale_modificoIlRegistroGeneraleInBaseAiRegistriRicevuti();
void RegistroGenerale_impostoRegistriRichiestiCompleti();
void RegistroGenerale_invioDateNecessarie(int sd);
void RegistroGenerale_aggiungiRegistroVuoto();
void RegistroGenerale_registroCorrente();
void RegistroGiornaliero_aggiungiEntry(char *type, int quantity);
void RegistroGenerale_creoListaDateNecessarie(struct tm period1, struct tm period2);
void RegistroGenerale_invioRegistriRichiesti(int sd);
void RegistroGenerale_ricevoDateNecessarieECreoRegistri(int sd);
void RegistroGenerale_ricevoRegistriRichiesti(int sd);
void RegistroGenerale_aggiungiAiRegistriRichiesti(struct RegistroGiornaliero *registro);
void RegistroGenerale_modificoRegistriRichiesti();
void RegistroGenerale_aggiungiRegistriRichiestiMancantiAlRegistroGenerale();
void RegistroGenerale_ricevoDateNecessarie(int sd);



/*ArchivioAggregazioni definitions*/
//CLEAN: it should be ok to have it as a list,
// as we wouldn't get the benefits of using a vector
struct ArchivioAggregazioni
{
    struct Aggregazione *lista_aggregazioni;
    char aggr[20], type[20], period[20];
} ArchivioAggregazioni;

struct Aggregazione
{
    char aggr[20], type[20], period[20];
    char intervallo_date[MAX_SIZE_RISULTATO][22];
    int risultato[MAX_SIZE_RISULTATO];
    int size_risultato;
    struct Aggregazione *next_aggregazione;
};

void ArchivioAggregazioni_stampaAggregazione(struct Aggregazione *pp);
struct Aggregazione *ArchivioAggregazioni_controlloPresenzaAggregazione(char *aggr, char *type, char *period);
void ArchivioAggregazioni_aggiungiAggregazione(char *aggr, char *type, char *period, int *risultato, char intervallo_date[MAX_SIZE_RISULTATO][22], int size_risultato);
void ArchivioAggregazioni_calcoloAggregazione(char *aggr, char *type, char *period);
int ArchivioAggregazioni_verificaPresenzaDati(int flood);
void ArchivioAggregazioni_precalcoloAggregazione(char *aggr, char *type, char *period);
int ArchivioAggregazioni_periodoValido(char *period);
void ArchivioAggregazioni_gestioneAggregazione(char *aggr, char *type, char *period);
void ArchivioAggregazioni_settaIntervalloDate(char intervallo_date[MAX_SIZE_RISULTATO][22], int indice, struct tm prima_data, struct tm seconda_data);