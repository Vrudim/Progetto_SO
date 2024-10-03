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
int timeout;

struct sembuf semlock = {0, -1, 0};
struct sembuf semunlock = {0, 1, 0};


//Svuoto la memoria e i semafori
void pulizia(int sig) {
  printf("Ricevuto il segnale %d chiudo il Server.", sig);
  
  //Rimuovo la memoria condivisa
  shmdt(grid);
  shmctl(shm_id, IPC_RMID, NULL);
  
  //Rimuovo i semafori
  semctl(sem_id, 0, IPC_RMID);
  
  exit(0);
}

//Inizio il giocco
void InizioGiocco(){
  for(int i = 0; i < DimCampo; i++){
    for(int j = 0; j< DimCampo; j++){
      grid[i][j] = ' ';
    }
  }
  
  grid[0][0] = 'O';
  grid[1][1] = 'X';
  
}

//Controllo il campo per i Triss
int ControlloVittoria(){
  
  for(int i=0; i < DimCampo; i++){
  //Controllo le Righe
  if(grid[i][0] != ' ' && grid[i][0] == grid[i][1] && grid[i][1] == grid[i][2])
    return 1;
  //Controllo le Colonne
  if(grid[0][i] != ' ' && grid[0][i] == grid[1][i] && grid[1][i] == grid[2][i])
    return 1;
  }
  
  //Controllo le Diagonali
  if(grid[0][0] != ' ' && grid[0][0] == grid[1][1] && grid[1][1] == grid[2][2])
    return 1;
  if(grid[0][2] != ' ' && grid[2][0] == grid[1][1] && grid[1][1] == grid[0][2])
    return 1;

  //Controllo se il giocco finisce in parita
  for(int i = 0; i < DimCampo; i++)
    for(int j =0; j < DimCampo; j++)
      if(grid[i][j] == ' ')
        return 0;
        
  return -1;
}

int main(int argc, char*argv[]){
  if(argc<4) {
    printf("Utilizzo: %s <timeout> <Simbolo Giocatore1> <Simbolo Giocatore2>\n", argv[0]);
    exit(1);
  }
  
  timeout = atoi(argv[1]);
  char giocatore1 = argv[2][0];
  char giocatore2 = argv[3][0];


  signal(SIGINT, pulizia);
  
  shm_id =shmget(SHM_Key, sizeof(char) * DimCampo * DimCampo, IPC_CREAT | 0666);
  if(shm_id == 1){
    perror("shmget");
    exit(1);
  }
  
  grid = shmat(shm_id, NULL, 0);
  if(grid == (char (*)[DimCampo]) - 1){
    perror("shmat");
    exit(1);
  }
  
  sem_id = semget(SEM_Key, 1, IPC_CREAT | 0666);
  if(sem_id == -1){
    perror("semget");
    exit(1);
  }
  
  semctl(sem_id, 0, SETVAL, 1);
  
  signal(SIGINT, pulizia);
  
  InizioGiocco();
  
  while(1) {
    
    semop(sem_id, &semlock, 1);
    
    int vincitore = ControlloVittoria();
    if(vincitore == 1){
      printf("Abbiamo un vincitore.\n");
      break;
    } else if(vincitore == 1){
      printf("La partita finisce in parita.\n");
      break;
    }
    
    semop(sem_id, &semunlock, 1);
    
    sleep(1);
  }
  
  shmdt(grid);
  shmctl(shm_id, IPC_RMID, NULL);
  semctl(sem_id, 0, IPC_RMID);
  
  return 0;
}
