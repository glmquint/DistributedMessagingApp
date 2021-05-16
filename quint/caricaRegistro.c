#include "Include.h"

struct RegistroGiornaliero *lista;

// FIXUP: refactor with definition in FileManager
void FileManager_salvaRegistro(int porta)
{

  struct RegistroGiornaliero *pp;
  FILE *fptr;
  char buffer[1024], tmp[20];
  strcpy(buffer, "./salvaRegistro");
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
    fwrite(pp, sizeof(struct RegistroGiornaliero), 1, fptr);
    pp = pp->next_registro;
  }
  fclose(fptr);
}

int main()
{
  int a, b, c, porta;
  struct RegistroGiornaliero *new, *ultimo;
  char buffer[10];
  struct tm tmp = {0};

  printf("Inserisci la porta\n");
  scanf("%d", &porta);
  printf("Digita si se vuoi uscire senza caricare un registro\n");
  scanf("%s", buffer);
  if (strcmp(buffer, "si") == 0)
  {
    FileManager_salvaRegistro(porta);
  }
  else
  {
    while (1)
    {
      new = malloc(sizeof(struct RegistroGiornaliero));

      printf("Inserisci una data\n");
      scanf("%d %d %d", &a, &b, &c);

      tmp.tm_mday = a;
      tmp.tm_mon = b - 1;
      tmp.tm_year = c - 1900;
      mktime(&tmp);
      new->data = tmp;
      printf("Inserisci il numero di tamponi di nuovi casi e se è completo\n");
      scanf("%d %d %d", &new->tamponi, &new->nuovi_casi, &new->completo);
      if (ultimo)
        ultimo->next_registro = new;
      if (lista == 0)
        lista = new;
      ultimo = new;

      printf("Digita si se vuoi uscire\n");
      scanf("%s", buffer);
      if (strcmp(buffer, "si") == 0)
        break;
    }

    FileManager_salvaRegistro(porta);
  }
  exit(0);
}