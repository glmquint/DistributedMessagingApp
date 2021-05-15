#include "Include.h"

//funzione che stampa l'aggregazione ricevuta come argomento
void ArchivioAggregazioni_stampaAggregazione(struct Aggregazione *pp)
{
    int i = 0;
    printf("Il risultato è: \n");
    for (i = 0; i < pp->size_risultato; i++)
    {
        printf("%s: ", pp->intervallo_date[i]);
        printf("%d\n", pp->risultato[i]);
    }
    printf("\n");
}

//funcione che controlla se un'aggregazione è presente nell'ArchivioAggregazioni
struct Aggregazione *ArchivioAggregazioni_controlloPresenzaAggregazione(char *aggr, char *type, char *period)
{
    struct Aggregazione *pp;

    printf("Controllo se l'aggregazione è presente nell'Archivio\n");
    pp = ArchivioAggregazioni.lista_aggregazioni;
    while (pp != 0)
    {
        if (strcmp(pp->aggr, aggr) == 0)
        {
            if (strcmp(pp->type, type) == 0)
            {
                if (strcmp(pp->period, period) == 0)
                {
                    printf("Aggregazione trovata\n");
                    return pp;
                }
            }
        }
        pp = pp->next_aggregazione;
    }
    printf("Aggregazione non trovata\n");
    return 0;
}

//funzione che aggiunge un'aggregazione all'ArchivioAggregazioni
void ArchivioAggregazioni_aggiungiAggregazione(char *aggr, char *type, char *period, int *risultato, char intervallo_date[MAX_SIZE_RISULTATO][22], int size_risultato)
{
    int i = 0;
    struct Aggregazione *pp;
    struct Aggregazione *new = malloc(sizeof(struct Aggregazione));
    strcpy(new->aggr, aggr);
    strcpy(new->type, type);
    strcpy(new->period, period);
    new->next_aggregazione = 0;
    new->size_risultato = size_risultato;
    for (i = 0; i < size_risultato; i++){
        new->risultato[i] = risultato[i];
        strcpy(new->intervallo_date[i], intervallo_date[i]);
    }

    pp = ArchivioAggregazioni.lista_aggregazioni;
    if (pp)
    {
        while (pp->next_aggregazione != 0)
        {
            pp = pp->next_aggregazione;
        }
        pp->next_aggregazione = new;
    }
    else
    {
        ArchivioAggregazioni.lista_aggregazioni = new;
    }
    printf("Aggregazione aggiunta all'Archivio\n");
    printf("Il risultato dell'aggregazione è: \n");
    for (i = 0; i < size_risultato; i++)
    {
        printf("%s: ", intervallo_date[i]);
        printf("%d\n", risultato[i]);
    }
    printf("\n");
}

//funzione che setta l'array di date nell'aggregazione che sto calcolando
void ArchivioAggregazioni_settaIntervalloDate(char intervallo_date[MAX_SIZE_RISULTATO][22], int indice, struct tm prima_data, struct tm seconda_data){
    char tmp[20];
    int somma = 0;
    sprintf(tmp, "%d", prima_data.tm_mday);
    strcpy(intervallo_date[indice], tmp);
    strcat(intervallo_date[indice], ":");
    somma = prima_data.tm_mon+1;
    sprintf(tmp, "%d", somma);
    strcat(intervallo_date[indice], tmp);
    strcat(intervallo_date[indice], ":");
    somma = prima_data.tm_year + 1900;
    sprintf(tmp, "%d", somma);
    strcat(intervallo_date[indice], tmp);
    strcat(intervallo_date[indice], "-");
    sprintf(tmp, "%d", seconda_data.tm_mday);
    strcat(intervallo_date[indice], tmp);
    strcat(intervallo_date[indice], ":");
    somma = seconda_data.tm_mon+1;
    sprintf(tmp, "%d", somma);
    strcat(intervallo_date[indice], tmp);
    strcat(intervallo_date[indice], ":");
    somma = seconda_data.tm_year+1900;
    sprintf(tmp, "%d", somma);
    strcat(intervallo_date[indice], tmp);
}

//funzione che calcola il risultato di una aggregazione richiesta
void ArchivioAggregazioni_calcoloAggregazione(char *aggr, char *type, char *period)
{
    struct RegistroGiornaliero *pp;
    int risultato[MAX_SIZE_RISULTATO] = {0}, ret, dato1 = 0, dato2 = 0;
    char intervallo_date[MAX_SIZE_RISULTATO][22];
    int size_risultato = 0;
    time_t primo, secondo, tmp;
    struct tm temporaneo, prima_data;
    primo = mktime(&RegistroGenerale.periodo_inizio);
    secondo = mktime(&RegistroGenerale.periodo_fine);
    pp = RegistroGenerale.lista_registri;

    //Scorro tutti i RegistriGiornalieri del RegistroGenerale. Se la data è compresa tra il periodo di inizio e di fine dell'aggregazione
    //aggiungo i suoi valori al calcolo. Se è un totale ne aggiungo i valori se è una variazione faccio la differenza tra il giorno corrente
    //e il giorno precedente
    printf("Calcolo l'aggregazione\n");
    if (strcmp("totale", aggr) == 0)
    {
        size_risultato = 1;
        while (pp != 0)
        {
            tmp = mktime(&pp->data);

            if (tmp >= primo && tmp <= secondo)
            {
                if (strcmp("tampone", type) == 0)
                {
                    risultato[0] += pp->tamponi;
                }
                else
                {
                    risultato[0] += pp->nuovi_casi;
                }
            }
            pp = pp->next_registro;
        }
        ArchivioAggregazioni_settaIntervalloDate(intervallo_date, 0, RegistroGenerale.periodo_inizio, RegistroGenerale.periodo_fine);
    }
    else
    {
        while (pp != 0)
        {
            ret = RegistroGenerale_comparaData(pp->data, RegistroGenerale.periodo_inizio);
            if (ret == -1 || ret == 0)
                break;
            else
                pp = pp->next_registro;
        }

        temporaneo = RegistroGenerale.periodo_inizio;
        while (1)
        {
            if (RegistroGenerale_comparaData(temporaneo, pp->data) == 0)
            {
                if (strcmp("tampone", type) == 0)
                    dato2 = pp->tamponi;
                else
                    dato2 = pp->nuovi_casi;
                pp = pp->next_registro;
            }
            else
                dato2 = 0;
            if (size_risultato != 0)
            {
                risultato[size_risultato - 1] = dato2 - dato1;
                ArchivioAggregazioni_settaIntervalloDate(intervallo_date, size_risultato-1, prima_data, temporaneo);
            }

            dato1 = dato2;
            prima_data = temporaneo;

            if (RegistroGenerale_comparaData(temporaneo, RegistroGenerale.periodo_fine) == 0)
                break;
            temporaneo.tm_mday++;
            mktime(&temporaneo);
            size_risultato++;
        }
    }
    printf("Aggregazione calcolata correttamente\n");
    ArchivioAggregazioni_aggiungiAggregazione(aggr, type, period, risultato, intervallo_date, size_risultato);
}

//funzione che verifica la presenza dei Registri Necessari o Richiesti. Se un registro è presente nel RegistroGenerale del peer
// ed è completo viene rimosso dalla lista delle Date Necessarie.
//Se il parametro flood == 1 significa che la funzione è stata chiamata da una gestioneFlood e quindi se un Registro
//viene trovato il peer deve essere aggiunto alla lista dei peer da contattare per ottenere i Registri Necessari.
//La funzione ritorna 1 se la Lista delle date Necessarie viene svuotata altrimenti torna 1
int ArchivioAggregazioni_verificaPresenzaDati(int flood)
{
    struct tm period1 = RegistroGenerale.lista_date_necessarie->data;
    struct tm period2;
    if(RegistroGenerale.lista_date_necessarie->next_data) 
        period2 = RegistroGenerale.lista_date_necessarie->next_data->data; //la seconda data della lista
    struct RegistroGiornaliero *pr = 0;
    struct DataNecessaria *pd = 0, *prevp = 0;
    pr = RegistroGenerale.lista_registri;

    printf("Verifico la presenza dei dati\n");
    //Se la prima data è * non posso rimuovere date dalle DateNecessarie mi limito ad aggiungere il peer alla lista dei peer da contattare
    //se il peer ha un registro che ha una data compresa tra 1 1 1970 e la seconda data
    if (flood == 1)
    {
        if (period1.tm_mday == 1 && period1.tm_mon == 0 && period1.tm_year == 70)
        {
            while (pr != 0)
            {
                if ((RegistroGenerale_comparaData(period1, pr->data) == 0 || RegistroGenerale_comparaData(period1, pr->data) == 1) && (RegistroGenerale_comparaData(pr->data, period2) == 0 || RegistroGenerale_comparaData(pr->data, period2) == 1))
                {
                    sPeer_aggiungiPeerDaContattare(sPeer.port);
                    printf("Registro trovato, Peer aggiunto ai peer da contattare\n");
                    return 0;
                }
                pr = pr->next_registro;
            }
        }
    }

    while (pr != 0)
    {
        prevp = 0;
        pd = RegistroGenerale.lista_date_necessarie;
        while (pd != 0 && RegistroGenerale_comparaData(pr->data, pd->data) != 0)
        {
            prevp = pd;
            pd = pd->next_data;
        }
        if (pd)
        {
            if (flood == 1){
                sPeer_aggiungiPeerDaContattare(sPeer.port);
            }    
            if (pr->completo == 1)
            {
                RegistroGenerale.numero_date_necessarie--;

                pd = pd->next_data;
                if (prevp == 0)
                    RegistroGenerale.lista_date_necessarie = pd;
                else
                    prevp->next_data = pd;
            }
        }

        pr = pr->next_registro;
    }

    if (RegistroGenerale.lista_date_necessarie == 0)
        return 1;
    else
        return 0;
}

//funzione che gestisce il calcolo dell'aggregazione nel caso in cui il non abbia trovato nel suo Archivio l'aggregazione richiesta
void ArchivioAggregazioni_precalcoloAggregazione(char *aggr, char *type, char *period)
{
    if (ArchivioAggregazioni_verificaPresenzaDati(0))
    {
        printf("Il peer HA tutte le date necessarie per calcolare l'aggregazione\n");
        ArchivioAggregazioni_calcoloAggregazione(aggr, type, period);
        return;
    }
    else
    {
        printf("Il peer NON HA tutte le date necessarie per calcolare l'aggregazione\n");
        printf("Inoltro richiesta ai neighbor per l'aggregazione\n");
        if (sPeer_richiestaAggregazioneNeighbor(aggr, type, period))
        {
            return; //se i miei vicini mi passano l'aggregazione l'aggiungo all'ArchivioAggregazioni e la stampo
        }
        else
        {
            printf("Aggregazione non fornita dai neighbor\n");
            strcpy(ArchivioAggregazioni.aggr, aggr);
            strcpy(ArchivioAggregazioni.type, type);
            strcpy(ArchivioAggregazioni.period, period);
            sPeer_sendFlood(); //setta la variabile sPeer.peer_da_contattare
        }
    }
}

//funzione che valuta in base alla stringa period ricevuta come parametro se il periodo inserito è valido.
//Se il periodo inserito è valido crea una stringa contenente il periodo richiesto nel formato corretto(trasforma l'* in una data valida)
//la funzione ritorna 1 se il periodo inserito è valido altrimenti torna 0
int ArchivioAggregazioni_periodoValido(char *period)
{
    char delim[] = ":-";
    char *ptr;
    int d[6] = {0};
    int i = 0, somma = 0;
    struct tm current_time, odierna, period1, period2;
    time_t prima, seconda, oggi;
    char tmp_mday[10], tmp_mon[10], tmp_year[10], buffer[50];

    printf("Analizzo il periodo inserito\n");
    strcpy(buffer, period);

    //creo le due date a partire dalla stringa letta come parametro se il parametro non era presente(" ") le due date create saranno
    //il 1 1 1970 e l'ultimo registro chiuso valido
    if (strcmp(buffer, " ") != 0)
    {
        ptr = strtok(buffer, delim);
        if (*ptr == '*')
        {
            for (i = 0; i < 3; i++)
            {
                d[i] = 0;
            }

            ptr = strtok(NULL, delim);
            while (ptr != 0 && i < 6)
            {
                sscanf(ptr, "%d", &d[i]);
                ptr = strtok(NULL, delim);
                i++;
            }
        }
        else
        {
            while (ptr != 0 && i < 3)
            {
                sscanf(ptr, "%d", &d[i]);
                ptr = strtok(NULL, delim);
                i++;
            }
            if(!ptr) return 0;
            if (*ptr == '*')
            {
                for (i = 3; i < 6; i++)
                {
                    d[i] = 0;
                }
            }
            else
            {
                while (ptr != 0 && i < 6)
                {
                    sscanf(ptr, "%d", &d[i]);
                    ptr = strtok(NULL, delim);
                    i++;
                }
            }
        }
    }
    else
        ;

    period1.tm_sec = 0;
    period1.tm_min = 0;
    period1.tm_hour = 0;
    period1.tm_isdst = -1;
    if (d[0] == 0)
    {
        period1.tm_mday = 1;
        period1.tm_mon = 0;
        period1.tm_year = 70;
    }
    else
    {
        period1.tm_mday = d[0];
        period1.tm_mon = d[1] - 1;
        period1.tm_year = d[2] - 1900;
    }

    period2.tm_sec = 0;
    period2.tm_min = 0;
    period2.tm_hour = 0;
    period2.tm_isdst = -1;
    if (d[3] == 0)
    {
        time(&oggi);
        current_time = *localtime(&oggi);
        if (current_time.tm_hour < 18)
        {
            current_time.tm_mday--;
            mktime(&current_time);
            period2.tm_mday = current_time.tm_mday;
            period2.tm_mon = current_time.tm_mon;
            period2.tm_year = current_time.tm_year;
        }
        else
        {
            period2.tm_mday = current_time.tm_mday;
            period2.tm_mon = current_time.tm_mon;
            period2.tm_year = current_time.tm_year;
        }
    }
    else
    {
        period2.tm_mday = d[3];
        period2.tm_mon = d[4] - 1;
        period2.tm_year = d[5] - 1900;
    }

    time(&rawtime);
    current_time = *localtime(&rawtime);
    odierna = current_time;
    odierna.tm_sec = 0;
    odierna.tm_min = 0;
    odierna.tm_hour = 0;

    oggi = mktime(&odierna);
    prima = mktime(&period1);
    seconda = mktime(&period2);

    if (prima > oggi)
        return 0;
    else
    {
        if (prima == oggi && current_time.tm_hour < 18)
            return 0;
    }

    if (seconda > oggi)
        return 0;
    else
    {
        if (seconda == oggi && current_time.tm_hour < 18)
            return 0;
    }

    printf("Il periodo inserito è valido\n");

    sprintf(tmp_mday, "%d", period1.tm_mday);
    somma = period1.tm_mon + 1; 
    sprintf(tmp_mon, "%d", somma);
    somma = period1.tm_year + 1900;
    sprintf(tmp_year, "%d", somma);
    strcpy(buffer, tmp_mday);
    strcat(buffer, ":");
    strcat(buffer, tmp_mon);
    strcat(buffer, ":");
    strcat(buffer, tmp_year);
    strcat(buffer, "-");

    sprintf(tmp_mday, "%d", period2.tm_mday);
    somma = period2.tm_mon + 1;
    sprintf(tmp_mon, "%d", somma);
    somma = period2.tm_year + 1900;
    sprintf(tmp_year, "%d", somma);
    strcat(buffer, tmp_mday);
    strcat(buffer, ":");
    strcat(buffer, tmp_mon);
    strcat(buffer, ":");
    strcat(buffer, tmp_year);

    strcpy(period, buffer);

    RegistroGenerale.periodo_inizio = period1;
    RegistroGenerale.periodo_fine = period2;
    RegistroGenerale_creoListaDateNecessarie(period1, period2);
    return 1;
}

//funzione che gestisce la richiesta di aggregazione da input
void ArchivioAggregazioni_gestioneAggregazione(char *aggr, char *type, char *period)
{
    struct Aggregazione *pp;
    if (ArchivioAggregazioni_periodoValido(period)) //la periodoValido modifica la stringa period nel modo giusto
    {
        pp = ArchivioAggregazioni_controlloPresenzaAggregazione(aggr, type, period);
        if (pp)
        {
            ArchivioAggregazioni_stampaAggregazione(pp);
            return;
        }
        else
        {
            ArchivioAggregazioni_precalcoloAggregazione(aggr, type, period);
        }
    }
    else
    {
        printf("Errore: periodo non valido\n\n");
        return;
    }
}