#include "Include.h"

#define MAX_SIZE_RISULTATO 100

struct Aggregazione *lista = 0;

// FIXUP: refactor with definition in FileManager
void FileManager_salvaArchivioAggregazioni(int porta)
{
    struct Aggregazione *pp;
    FILE *fptr;
    char buffer[1024], tmp[20];
    strcpy(buffer, "./salvaArchivio");
    sprintf(tmp, "%d", porta);
    strcat(buffer, tmp);

    if ((fptr = fopen(buffer, "w")) == NULL)
    {
        printf("Error! opening file");

        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    pp = lista;
    while (pp != 0)
    {
        fwrite(pp, sizeof(struct Aggregazione), 1, fptr);
        pp = pp->next_aggregazione;
    }
    fclose(fptr);
}

int main()
{
    int i, porta;
    struct Aggregazione *new = 0, *ultimo = 0;
    char buffer[10];

    printf("Inserisci la porta\n");
    scanf("%d", &porta);
    printf("Digita si se vuoi uscire senza caricare un archivio\n");
    scanf("%s", buffer);
    if (strcmp(buffer, "si") == 0)
    {
        FileManager_salvaArchivioAggregazioni(porta);
    }
    else
    {
        while (1)
        {
            new = malloc(sizeof(struct Aggregazione));
            printf("Inserisci un aggr type e period\n");
            scanf("%s %s %s", new->aggr, new->type, new->period);

            printf("Inserisci il size_risultato\n");
            scanf("%d", &new->size_risultato);
            printf("Inserisci il risultato\n");
            for (i = 0; i < new->size_risultato; i++)
            {
                scanf("%d", &new->risultato[i]);
            }

            if (ultimo)
                ultimo->next_aggregazione = new;
            if (lista == 0)
                lista = new;

            ultimo = new;

            printf("Digita si se vuoi uscire\n");
            scanf("%s", buffer);
            if (strcmp(buffer, "si") == 0)
                break;
        }
        FileManager_salvaArchivioAggregazioni(porta);
    }
    exit(0);
}