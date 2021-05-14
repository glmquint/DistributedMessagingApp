#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define STDIN 0
#define TIMEOUT 1              /*tempo che il peer aspetta per ricevere la risposta al boot dal server*/
#define BOOT_MSG 6            /*"10000\0"*/
#define BOOT_RESP 12           /*"10001 10002\0"*/
#define MAX_SIZE_RISULTATO 100 /*definisce la dimesione massima dellarray del risultato delle Aggregazioni*/
#define NUMERO_VICINI 2
#define REQ_LEN 6    /* REQAG\0 REQRE\0 FLOOD\0*/
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

/*IOController definitions*/
struct IOController
{
    int requester;
    int sd_flood;
    int richieste_inviate;
    char richiesta_id[20];
    int richiesta_in_gestione;
    int risposte_mancanti;
    fd_set master;
    fd_set read_fds;
    fd_set write_fds;
    int fdmax;
    int vicini[10];
    int porta;
    int peer_da_contattare[MAX_PEER_DA_CONTATTARE];
    int numero_peer_da_contattare;
    int ds_port;
    char ds_ip[20];
} IOController;

void IOController_help();
void IOController_serverBoot(char *ds_addr, int ds_port);
int IOController_richiestaAggregazioneNeighbor(char *aggr, char *type, char *period);
void IOController_gestioneRichiestaAggregazione(int sd);
void IOController_richiestaRegistri();
void IOController_stop();
void IOController_aggiungiPeerDaContattare(int peer);
void IOController_richiestaFlood(int requester);
void IOController_invioRispostaFlood();
void IOController_ricezioneRispostaFlood(int sd);
void IOController_sendFlood();
int IOController_verificaRichiestaRicevuta(char richiesta_id[50], int sd);
void IOController_gestioneFlood(int sd);
void IOController_menu();
void IOController_gestioneRichiestaRegistri(int sd);
void IOController_gestisciIO();
void IOController_gestisciStop();
void IOController_disconnessioneNetwork();
void IOController_gestisciNuoviVicini(int sd);



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