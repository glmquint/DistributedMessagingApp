#include "Include.h"

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

/* Database che racchiude e gestisce le entry di nuovi casi/tamponi inseriti dai peer */

//funzione che compara le due date passate come parametro 
//ritorna 0 se le due date passate come parametro sono uguali, ritorna 1 se la seconda è maggiore della prima altrimenti torna -1
int RegistroGenerale_comparaData(struct tm prima_data, struct tm seconda_data)
{
    struct tm prima_data_pulita = {0, 0, 0, prima_data.tm_mday, prima_data.tm_mon, prima_data.tm_year};
    struct tm seconda_data_pulita = {0, 0, 0, seconda_data.tm_mday, seconda_data.tm_mon, seconda_data.tm_year};

    time_t prima, seconda;

    prima = mktime(&prima_data_pulita);
    seconda = mktime(&seconda_data_pulita);

    DEBUG_PRINT(("in comparaData\tprima_data: %d\tseconda_data: %d\n", (int)prima, (int)seconda));

    if (seconda >= prima)
    {
        if (seconda == prima)
            return 0;
        else
            return 1;
    }
    else
        return -1;
}

//funzione che aggiunge al RegistroGenerale quei registri ricevuti dai peer che non sono già presenti nel RegistroGenerale
void RegistroGenerale_aggiungiRegistriRichiestiMancantiAlRegistroGenerale()
{
    struct RegistroGiornaliero *pp, *prevp, *rr;
    struct RegistroGiornaliero *new;

    prevp = 0;
    pp = RegistroGenerale.lista_registri;
    rr = RegistroGenerale.lista_registri_richiesti;

    printf("Aggiungo al RegistroGenerale quelle date che non sono già presenti\n");

    while (rr != 0)
    {
        while (pp != 0 && RegistroGenerale_comparaData(rr->data, pp->data) != 1)
        {
            prevp = pp;
            pp = pp->next_registro;
        }

        if (prevp)
        {
            if (RegistroGenerale_comparaData(prevp->data, rr->data) != 0)
            {
                new = malloc(sizeof(struct RegistroGiornaliero));
                new->data = rr->data;
                new->nuovi_casi = rr->nuovi_casi;
                new->tamponi = rr->tamponi;
                new->completo = rr->completo;

                prevp->next_registro = new;
                new->next_registro = pp;
                pp = new;
            }
        }
        else
        {
            new = malloc(sizeof(struct RegistroGiornaliero));
            new->data = rr->data;
            new->nuovi_casi = rr->nuovi_casi;
            new->tamponi = rr->tamponi;
            new->completo = rr->completo;

            RegistroGenerale.lista_registri = new;
            new->next_registro = pp;
            pp = new;
        }

        rr = rr->next_registro;  
    }
}

//funzione che modifica il RegistroGenerale in base ai Registri Ricevuti dai peer contattati
void RegistroGenerale_modificoIlRegistroGeneraleInBaseAiRegistriRicevuti()
{
    struct RegistroGiornaliero *pr, *rr;
    pr = RegistroGenerale.lista_registri;
    rr = RegistroGenerale.lista_registri_richiesti;

    printf("Modifico il RegistroGenerale in base ai registri ricevuti\n");
    while (pr != 0)
    {
        rr = RegistroGenerale.lista_registri_richiesti;
        while (rr != 0 && RegistroGenerale_comparaData(pr->data, rr->data) != 0)
        {
            rr = rr->next_registro;
        }
        if (rr)
        {
            if (pr->completo != 1)
            {
                if (rr->completo)
                {
                    pr->completo = rr->completo;
                    pr->nuovi_casi = rr->nuovi_casi;
                    pr->tamponi = rr->tamponi;
                }
                else
                {
                    pr->completo = rr->completo;
                    pr->nuovi_casi += rr->nuovi_casi;
                    pr->tamponi += rr->tamponi;
                }
            }
        }
        pr = pr->next_registro;
    }

    RegistroGenerale_aggiungiRegistriRichiestiMancantiAlRegistroGenerale();
}

//funzione che imposta i registri richiesti dopo una Flood come completi
void RegistroGenerale_impostoRegistriRichiestiCompleti()
{
    struct RegistroGiornaliero *pr;
    pr = RegistroGenerale.lista_registri;
    struct tm period1 = RegistroGenerale.periodo_inizio;
    struct tm period2 = RegistroGenerale.periodo_fine;

    printf("Imposto i registri richiesti come completi\n");
    while (pr != 0)
    {
        if((RegistroGenerale_comparaData(period1, pr->data) == 0 || RegistroGenerale_comparaData(period1, pr->data) == 1) && (RegistroGenerale_comparaData(pr->data, period2) == 0 || RegistroGenerale_comparaData(pr->data, period2) == 1))
        {
            pr->completo = 1;
        }
        pr = pr->next_registro;
    }
}

//funzione che invia ad un peer sul socket sd le Date Nenessarie
void RegistroGenerale_invioDateNecessarie(int sd)
{
    uint16_t lmsg;
    int ret, len;
    struct DataNecessaria *pp;
    char tmp_mday[10], tmp_mon[10], tmp_year[10], buffer[20];

    printf("Invio le date Necessarie\n");
    pp = RegistroGenerale.lista_date_necessarie;

    lmsg = htons(RegistroGenerale.numero_date_necessarie);
    ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }
    while (pp)
    {
        sprintf(tmp_mday, "%d", pp->data.tm_mday);
        sprintf(tmp_mon, "%d", pp->data.tm_mon);
        sprintf(tmp_year, "%d", pp->data.tm_year);
        strcpy(buffer, tmp_mday);
        strcat(buffer, " ");
        strcat(buffer, tmp_mon);
        strcat(buffer, " ");
        strcat(buffer, tmp_year);

        len = strlen(buffer) + 1;
        lmsg = htons(len);
        ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if(ret < 0){
            perror("Errore in fase di invio: ");
            exit(1);
        }
        ret = send(sd, (void *)buffer, len, 0);
        if(ret < 0){
            perror("Errore in fase di invio: ");
            exit(1);
        }

        pp = pp->next_data;
    } 
}

//funzione che aggiunge un RegistroGiornaliero con la data corrente al RegistroGenerale
void RegistroGenerale_aggiungiRegistroVuoto()
{
    time(&rawtime);
    struct tm current_time;
    current_time = *localtime(&rawtime);
    struct RegistroGiornaliero *nuovo = malloc(sizeof(struct RegistroGiornaliero));

    printf("Aggiungo il un registro con data corrente al RegistroGenerale\n");
    
    nuovo->completo = 0;
    nuovo->data = current_time;
    nuovo->nuovi_casi = 0;
    nuovo->tamponi = 0;
    nuovo->next_registro = 0;

    if (RegistroGenerale.lista_registri == 0)
    {
        RegistroGenerale.lista_registri = nuovo;
        RegistroGenerale.ultimo_registro = nuovo;
        return;
    }
    else
    {
        RegistroGenerale.ultimo_registro->next_registro = nuovo;
        RegistroGenerale.ultimo_registro = nuovo;
    }
}

//funzione che imposta la variabile RegistroGenerale.ultimo_registro sul RegistroGiornaliero corretto per l'inserimento di una nuova entry
void RegistroGenerale_registroCorrente()
{
    time(&rawtime);
    struct tm current_time, day_after;
    current_time = *localtime(&rawtime);
    day_after = *localtime(&rawtime);
    int ret;

    printf("Stabilisco qual'è il RegistroGiornaliero corretto in cui inserire la entry\n");
    if (RegistroGenerale.ultimo_registro != 0)
        ret = RegistroGenerale_comparaData(RegistroGenerale.ultimo_registro->data, current_time);
    else
    {
        if (current_time.tm_hour < 18)
            ret = 1;
        else
            ret = 0;
    }

    if (ret == 0)
    {
        if (current_time.tm_hour < 18)
            return; // non faccio niente perchè RegistroGenerale.ultimo_registro punta già al RegistroGiornaliero corretto
        else
        {
            RegistroGenerale_aggiungiRegistroVuoto();
            day_after.tm_mday = day_after.tm_mday + 1;
            mktime(&day_after);
            RegistroGenerale.ultimo_registro->data = day_after; //assegno al nuovo RegistroGiornaliero creato la data del giorno successivo a current_time
        }
    }
    else if (ret == -1)
    {
        return; //non faccio niente perchè se il RegistroGenerale.ultimo_registro punta ad un RegistroGiornaliero con una data maggiore della data corrente vuol dire
        //che mi trovo dopo le 18 del giorno prima della data del RegistroGenerale.ultimo_registro.Esempio: se chiamo la funzione RegistroGenerale_registroCorrente alle 19
        //del 10/12/2020 e RegistroGenerale.ultimo_registro ha data 11/12/2020 il RegistroGiornaliero puntato è quello giusto.
    }
    else if (ret == 1)
    {
        RegistroGenerale_aggiungiRegistroVuoto(); //aggiungo un RegistroGiornaliero che sarà puntato da RegistroGenerale.ultimo_registro
    }
}

//funzione che aggiunge una entry nel RegistroGiornaliero puntato dalla variabile RegistroGenerale.ultimo_registro
void RegistroGiornaliero_aggiungiEntry(char *type, int quantity)
{
    //aggiungo una entry al registro giornaliero
    RegistroGenerale_registroCorrente(); //funzione che si accerta che RegistroGenerale.ultimo_registro punti al registro corretto
    printf("Aggiungo la entry al RegistroGiornaliero corretto\n\n");
    if (strcmp("tampone", type) == 0)
    {
        RegistroGenerale.ultimo_registro->tamponi += quantity;
    }
    else
    {
        RegistroGenerale.ultimo_registro->nuovi_casi += quantity;
    }
}

//funnzione che crea la lista delle date necessarie in base alle date ricevute da una richiesta di Flood
void RegistroGenerale_creoListaDateNecessarie(struct tm period1, struct tm period2)
{

    struct DataNecessaria *pp;
    struct DataNecessaria *new_data = malloc(sizeof(struct DataNecessaria));
    struct tm tmp = {0};
    RegistroGenerale.lista_date_necessarie = 0;
    RegistroGenerale.numero_date_necessarie = 0;

    printf("Creo la lista delle date Necessarie\n");
    new_data->data = period1;
    new_data->next_data = 0;
    RegistroGenerale.lista_date_necessarie = new_data;
    RegistroGenerale.numero_date_necessarie++;

    if (period1.tm_mday == 1 && period1.tm_mon == 0 && period1.tm_year == 70)
    { //se la prima data è asterisco nelle lista metto solo le due date

        new_data = malloc(sizeof(struct DataNecessaria));

        new_data->data = period2;
        new_data->next_data = 0;
        RegistroGenerale.lista_date_necessarie->next_data = new_data;
        RegistroGenerale.numero_date_necessarie++;
        return;
    }

    pp = RegistroGenerale.lista_date_necessarie;
    while (1)
    {
        if (pp->data.tm_mday == period2.tm_mday && pp->data.tm_mon == period2.tm_mon && pp->data.tm_year == period2.tm_year)
            break;
        new_data = malloc(sizeof(struct DataNecessaria));
        tmp = pp->data;
        tmp.tm_mday += 1;
        mktime(&tmp);
        new_data->data = tmp;
        new_data->next_data = 0;
        pp->next_data = new_data;
        pp = pp->next_data;
        RegistroGenerale.numero_date_necessarie++;
    }

    return;
}

//funzione che invia i RegistriGiornalieri richiesti a chi li ha richiesta o in caso di stop
void RegistroGenerale_invioRegistriRichiesti(int sd)
{
    uint16_t lmsg;
    int ret, len;
    struct RegistroGiornaliero *pp;
    char tmp[10], buffer[50];

    printf("Invio i RegistriGiornalieri richiesti\n\n");
    pp = RegistroGenerale.lista_registri_richiesti;

    lmsg = htons(RegistroGenerale.numero_registri_richiesti);
    ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }
    while (pp != 0)
    {
        sprintf(tmp, "%d", pp->data.tm_mday);
        strcpy(buffer, tmp);
        strcat(buffer, " ");
        sprintf(tmp, "%d", pp->data.tm_mon);
        strcat(buffer, tmp);
        strcat(buffer, " ");
        sprintf(tmp, "%d", pp->data.tm_year);
        strcat(buffer, tmp);
        strcat(buffer, " ");
        sprintf(tmp, "%d", pp->nuovi_casi);
        strcat(buffer, tmp);
        strcat(buffer, " ");
        sprintf(tmp, "%d", pp->tamponi);
        strcat(buffer, tmp);
        strcat(buffer, " ");
        sprintf(tmp, "%d", pp->completo);
        strcat(buffer, tmp);

        len = strlen(buffer) + 1;
        lmsg = htons(len);
        ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        ret = send(sd, (void *)buffer, len, 0);

        pp = pp->next_registro;
        DEBUG_PRINT(("registro inviato\n"));
    }
    ret = recv(sd, (void*)&lmsg, sizeof(uint16_t), 0);
    if(ret < 0){
        perror("Errore in fase di ricezione: ");
        exit(1);
    }
}

//funzione che riceve le date necessarie al requester e crea una lista di RegistriGiornalieri richiesti
void RegistroGenerale_ricevoDateNecessarieECreoRegistri(int sd)
{
    int i, tmp_mday, tmp_mon, tmp_year, ret, len;
    struct RegistroGiornaliero *new, *pp;
    uint16_t lmsg;
    struct tm tmp;
    char buffer[11];

    printf("Ricevo le date necessarie e creo una lista di Registri Giornalieri richiesti\n");
    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }
    RegistroGenerale.numero_registri_richiesti = ntohs(lmsg);

    pp = 0;

    for (i = 0; i < RegistroGenerale.numero_registri_richiesti; i++)
    {
        ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(1);
        }
        len = ntohs(lmsg);
        ret = recv(sd, (void *)buffer, len, 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(1);
        }
        sscanf(buffer, "%d %d %d", &tmp_mday, &tmp_mon, &tmp_year);
        new = malloc(sizeof(struct RegistroGiornaliero));
        
        tmp.tm_sec = 0;
        tmp.tm_min = 0;
        tmp.tm_hour = 0;
        tmp.tm_mday = tmp_mday;
        tmp.tm_mon = tmp_mon;
        tmp.tm_year = tmp_year;
        tmp.tm_isdst = -1;
        mktime(&tmp);
        new->data = tmp;
        new->nuovi_casi = 0;
        new->tamponi = 0;
        new->completo = 0;

        if (pp)
            pp->next_registro = new;
        else
            RegistroGenerale.lista_registri_richiesti = new;
        pp = new;
    }  
}

//funzione che riceve sul socket sd i registri richiesti pieni di informazioni utili
void RegistroGenerale_ricevoRegistriRichiesti(int sd)
{
    int i, tmp_mday, tmp_mon, tmp_year, ret, len, nuovi_casi, tamponi, completo;
    struct RegistroGiornaliero *new, *pp;
    uint16_t lmsg;
    struct tm tmp;
    char buffer[50];

    printf("Ricevo i Registri richiesti\n");
    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione1: ");
        exit(1);
    }
    RegistroGenerale.numero_registri_richiesti = ntohs(lmsg);

    pp = 0;

    for (i = 0; i < RegistroGenerale.numero_registri_richiesti; i++)
    {
        ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione2: ");
            exit(1);
        }
        len = ntohs(lmsg);
        ret = recv(sd, (void *)buffer, len, 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione3: ");
            exit(1);
        }
        sscanf(buffer, "%d %d %d %d %d %d", &tmp_mday, &tmp_mon, &tmp_year, &nuovi_casi, &tamponi, &completo);
        new = malloc(sizeof(struct RegistroGiornaliero));
        tmp.tm_sec = 0;
        tmp.tm_min = 0;
        tmp.tm_hour = 0;
        tmp.tm_mday = tmp_mday;
        tmp.tm_mon = tmp_mon;
        tmp.tm_year = tmp_year;
        tmp.tm_isdst = -1;
        mktime(&tmp);
        new->data = tmp;
        new->nuovi_casi = nuovi_casi;
        new->tamponi = tamponi;
        new->completo = completo;

        if (pp)
            pp->next_registro = new;
        else
            RegistroGenerale.lista_registri_richiesti = new;
        pp = new;
    }
    ret = send(sd, (void*)&lmsg, sizeof(uint16_t), 0);
    if(ret < 0){
        perror("Errore in fase di invio: ");
        exit(1);
    } 
    
}

//funzione che aggiunge un RegistroGiornaliero passato come argomento alla lista dei registri richiesti
void RegistroGenerale_aggiungiAiRegistriRichiesti(struct RegistroGiornaliero *registro)
{
    struct RegistroGiornaliero *pp, *prevp;
    struct RegistroGiornaliero *new = malloc(sizeof(struct RegistroGiornaliero));
    pp = RegistroGenerale.lista_registri_richiesti;
    prevp = 0;
    while (pp != 0 && RegistroGenerale_comparaData(registro->data, pp->data) != 1)
    {
        prevp = pp;
        pp = pp->next_registro;
    }

    new->data = registro->data;
    new->nuovi_casi = registro->nuovi_casi;
    new->tamponi = registro->tamponi;
    new->completo = registro->completo;
    prevp->next_registro = new;
    new->next_registro = pp;
    RegistroGenerale.numero_registri_richiesti++;
}

//funzione che modifica la lista dei RegistriGiornalieri richiesti in base ai RegistriGiornalieri presenti nel RegistroGenerale
void RegistroGenerale_modificoRegistriRichiesti()
{
    struct tm period1 = RegistroGenerale.lista_registri_richiesti->data;
    struct tm period2;
    struct RegistroGiornaliero *pr, *pd;
    if(RegistroGenerale.lista_registri_richiesti->next_registro)
        period2 = RegistroGenerale.lista_registri_richiesti->next_registro->data; //la seconda data della lista
    
    pr = RegistroGenerale.lista_registri;
    pd = RegistroGenerale.lista_registri_richiesti;

    printf("Modifico i registri richiesti in base ai registri presenti nel RegistroGenerale\n");

    if (period1.tm_mday == 1 && period1.tm_mon == 0 && period1.tm_year == 70)
    {
        while (pr != 0)
        {
            if ((RegistroGenerale_comparaData(period1, pr->data) == 0 || RegistroGenerale_comparaData(period1, pr->data)) && (RegistroGenerale_comparaData(pr->data, period2) == 0 || RegistroGenerale_comparaData(pr->data, period2)))
            {
                RegistroGenerale_aggiungiAiRegistriRichiesti(pr);//se trovo un registro utile ma la prima data è quella del 1 1 1970 non ho 
                //una vera lista di registri richiesti da modificare dunque aggiungo direttamente il RegistroGiornaliero alla lista dei registri richiesti
            }
            pr = pr->next_registro;
        }
        return;
    }

    while (pr != 0)
    {
        pd = RegistroGenerale.lista_registri_richiesti;
        while (pd != 0 && RegistroGenerale_comparaData(pr->data, pd->data) != 0)
        {
            pd = pd->next_registro;
        }
        if (pd)
        {
            pd->nuovi_casi = pr->nuovi_casi;
            pd->tamponi = pr->tamponi;
            pd->completo = pr->completo;
        }
        pr = pr->next_registro;
    }
}

//funzione che riceve le date necessarie 
void RegistroGenerale_ricevoDateNecessarie(int sd)
{
    int i, tmp_mday, tmp_mon, tmp_year, ret, len;
    struct DataNecessaria *new, *pp;
    uint16_t lmsg;
    struct tm tmp;
    char buffer[11];

    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

    printf("Ricevo le date necessarie e creo la lista delle date Necessarie\n");
    RegistroGenerale.numero_date_necessarie = ntohs(lmsg);

    pp = 0;

    for (i = 0; i < RegistroGenerale.numero_date_necessarie; i++)
    {
        ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(1);
        }
        len = ntohs(lmsg);
        ret = recv(sd, (void *)buffer, len, 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(1);
        }
        sscanf(buffer, "%d %d %d", &tmp_mday, &tmp_mon, &tmp_year);
        new = malloc(sizeof(struct DataNecessaria));
        tmp.tm_sec = 0;
        tmp.tm_min = 0;
        tmp.tm_hour = 0;
        tmp.tm_mday = tmp_mday;
        tmp.tm_mon = tmp_mon;
        tmp.tm_year = tmp_year;
        tmp.tm_isdst = -1;
        mktime(&tmp);
        new->data = tmp;
        new->next_data = 0;
        if (pp)
            pp->next_data = new;
        else
            RegistroGenerale.lista_date_necessarie = new;
        pp = new;
    }
}