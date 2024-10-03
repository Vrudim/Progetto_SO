#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <string.h>

#define SHM_Key 1234
#define SEM_Key 5678
#define DimCampo 3

int shm_id, sem_id;
char (*grid)[DimCampo];
char symbol;

struct sembuf semlock = {0, -1, 0};
struct sembuf semunlock = {0, 1, 0};

void segnale(int sig) {
  printf("Ricevuto il segnale %d. Chiudo la partita. \n", sig);
  shmdt(grid);
  exit(0);
}

//Stampo a vista il Campo da Giocco
void Campo(){
  for(int i = 0; i<DimCampo; i++){
    for(int j = 0; j<DimCampo; j++){
      printf(" %c ", grid[i][j]);
      if(j < DimCampo-1)
        printf("|");
    }
    printf("\n");
    if(i < DimCampo-1)
      printf("---|---|---\n");
  }
  printf("\n");
}

void Giocca(){
  int riga, colona;
  
  while(1){
    printf("Inserisci la riga(0-2) e colona(0-2) dove inserire il simbolo %c : ", symbol);
    scanf("%d %d", &riga, &colona);
    
    if(riga < 0 || riga >= DimCampo || colona < 0 || colona > DimCampo){
      printf("Coordinate non valide.\n");
      continue;
    }
    
    if(grid[riga][colona] == ' '){
      grid[riga][colona] = symbol;
      break;
    } else {
      printf("Mossa invalida. Posizione gia occupata.\n");
    }
  }
}

int main(int argc, char*argv[]){
  if(argc < 2){
    printf("Utilizzo: %s <NomeGiocatore>\n", argv[0]);
    exit(1);
  }
  
  char *giocatore = argv[1];
  printf("Il giocatore %s e pronto.\n", giocatore);
  
  shm_id = shmget(SHM_Key, sizeof(char) * DimCampo * DimCampo, 0666);
  if(shm_id == -1){
    perror("shmget 1");
    exit(1);
  }
  
  grid = shmat(shm_id, NULL, 0);
  if(grid == (char(*)[DimCampo]) -1){
    perror("shmat");
    exit(1);
  }
  
  sem_id = semget(SEM_Key, 1, 0666);
  if(sem_id == -1){
    perror("semget");
    exit(1);
  }
  
  signal(SIGINT, segnale);
  
  symbol = (giocatore[0] == 'A')?'O':'X';
  
  while(1){
    
    semop(sem_id, &semlock, 1);
    
    Campo();
    
    Giocca();
    
    semop(sem_id, &semunlock, 1);
    
    sleep(1);
    
  }
  
  return 0;  
}
