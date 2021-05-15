#include "Include.h"

//funzione che salva le Aggregazioni presenti nell'ArchivioAggregazioni sul file salvaArchivio(numeroPeer)
void FileManager_salvaArchivioAggregazioni()
{
    struct Aggregazione *pp;
    FILE *fptr;
    char buffer[1024], tmp[20];
    strcpy(buffer, "./salvaArchivio");
    sprintf(tmp, "%d", sPeer.port);
    strcat(buffer, tmp);

    printf("Salvo le aggregazioni presenti nell'ArchivioAggregazioni del Peer %d\n", sPeer.port);

    if ((fptr = fopen(buffer, "w")) == NULL)
    {
        printf("Error! opening file");

        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    pp = ArchivioAggregazioni.lista_aggregazioni;
    while (pp != 0)
    {
        fwrite(pp, sizeof(struct Aggregazione), 1, fptr);
        pp = pp->next_aggregazione;
    }
    fclose(fptr);
}

//funzione che salva sul file salvaRegistro(numeroPeer) i Registri del RegistroGenerale presenti in memoria.
//il parametro ds è impostato a 1 quando è il ds a odinare al peer di chiudersi.
//Se il parametro è 0 la salvaRegistro è chiamata dalla stop e il peer salva su file i suoi registri solo se non li ha potuti trasmettere a 
//nessun vicino 
void FileManager_salvaRegistro(int ds)
{
    struct RegistroGiornaliero *pp;
    FILE *fptr;
    int trovato = 0, i;
    char buffer[1024], tmp[10];
    strcpy(buffer, "./salvaRegistro");
    sprintf(tmp, "%d", sPeer.port);
    strcat(buffer, tmp);

    if ((fptr = fopen(buffer, "w")) == NULL)
    {
        printf("Error! opening file");
        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    for(i = 0; i < CVECTOR_TOTAL((sPeer.vicini)); i++){
        if(CVECTOR_GET((sPeer.vicini), int, i)!=0) 
            trovato = 1;
    }

    if(!trovato || ds){
        printf("Salvo sul file %s i Registri presenti in memoria\n", buffer);
        pp = RegistroGenerale.lista_registri;
        while (pp != 0)
        {
            fwrite(pp, sizeof(struct RegistroGiornaliero), 1, fptr);
            pp = pp->next_registro;
        }
    }
    else{
        printf("I registri sono stati trasmessi ad un vicino quindi non li salvo in memoria\n");
    }
    
    fclose(fptr);
}

//funzione che carica in memoria le Aggregazioni presenti sul file salvaArchivio(numeroPeer)
void FileManager_caricaArchivioAggregazioni()
{

    struct Aggregazione *ultima_aggregazione;
    struct Aggregazione pp;
    struct Aggregazione *new_aggr;
    FILE *fptr;

    char buffer[1024], tmp[20];
    strcpy(buffer, "./salvaArchivio");
    sprintf(tmp, "%d", sPeer.port);
    strcat(buffer, tmp);

    printf("Carico in memoria le aggregazioni presenti sul file %s\n", buffer);

    if ((fptr = fopen(buffer, "r")) == NULL)
    {
        printf("Error! opening file");

        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    
    while (fread(&pp, sizeof(struct Aggregazione), 1, fptr))
    {
        new_aggr = malloc(sizeof(struct Aggregazione));
        *new_aggr = pp;
        if (ArchivioAggregazioni.lista_aggregazioni == 0)
            ArchivioAggregazioni.lista_aggregazioni = new_aggr;
        if (ultima_aggregazione)
            ultima_aggregazione->next_aggregazione = new_aggr;

        ultima_aggregazione = new_aggr;
    }

    fclose(fptr);
}

//funzione che carica in memoria i registri presenti sul file salvaRegistro(numeroPeer)
void FileManager_caricaRegistro()
{
    FILE *fptr;
    struct RegistroGiornaliero pp;
    struct RegistroGiornaliero *new;
    char buffer[1024], tmp[20];
    strcpy(buffer, "./salvaRegistro");
    sprintf(tmp, "%d", sPeer.port);
    strcat(buffer, tmp);

    if ((fptr = fopen(buffer, "r")) == NULL)
    {
        perror("Error! opening file");

        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    printf("Carico in memoria i file presenti sul file %s\n", buffer);
    
    while (fread(&pp, sizeof(struct RegistroGiornaliero), 1, fptr))
    {
        new = malloc(sizeof(struct RegistroGiornaliero));
        *new = pp;
        if (RegistroGenerale.lista_registri == 0)
            RegistroGenerale.lista_registri = new;
        if (RegistroGenerale.ultimo_registro)
            RegistroGenerale.ultimo_registro->next_registro = new;

        RegistroGenerale.ultimo_registro = new;
    }
    fclose(fptr);
}