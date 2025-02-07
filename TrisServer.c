/************************************
*Matricola                VR422249
*Nome e cognome           Vadim Musteata
*Data di realizzazione    07/02/2025
*************************************/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <stdbool.h>

#define Max_Dim 10
#define SHM_KEY 1234
#define SEM_KEY 5678

// Implemento una struttura per la memoria condivisa
typedef struct {
  int timeout; // E il tempo in secondi che ha un giocatore per efettuare una mossa
  int dimmensione; //Dimensione Matrice
  char matrice[Max_Dim][Max_Dim]; //Matrice di gioco
  int giocatoreattuale; // Memorizza 0 o 1 per i turni dei giocatori
  int vincitore; // Indica il vincitore
  int numeromosse; // Indica il numero di mosse effettuate
  int numeroturni; // Indica i numeri di turni che i giocatori hanno effettuato
  int maxmosse; //Indica il numero massimo di mosse possibili
  char simboli[2]; // Memorizza i simboli dei giocatori
  int numerogioccatori; // Memorizza il numero di giocatori presenti
  bool server; // Controllo per server
  bool GAttivo[2]; // Aggiunto per gestire abbandoni
  bool SinglePlayer; // Vero se il giocatore vuole giocare da solo
} MemoriaCondivisa;



int shm_id, sem_id;
MemoriaCondivisa *MemCond = NULL;

// Funzioni per la gestione dei semafori
void semaphore_op(int sem_id, int sem_num, int op) {
  struct sembuf sops = {sem_num, op, 0};
  semop(sem_id, &sops, 1);
}

void lock_semaphore(int sem_id, int sem_num) {
    semaphore_op(sem_id, sem_num, -1);
}

void unlock_semaphore(int sem_id, int sem_num) {
  semaphore_op(sem_id, sem_num, 1);
}

// funzione di chiusura server
void chiusura(){
    MemCond->server = false;
    printf("\nPulizia delle risorse.\n");
    if (shmdt(MemCond) == -1) perror("shmdt");
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) perror("shmctl");
    if (semctl(sem_id, 0, IPC_RMID) == -1) perror("semctl");
}



// Gestione Segnale CTRL+C
void handle_sigint(int sig) {

   static int CTRC = 0;
    CTRC++;
    
    if (CTRC == 1) {
        printf("\nPremi di nuovo CTRL-C per terminare la partita.\n");
    } else if (CTRC == 2) {
        printf("\nTerminazione forzata. Pulizia risorse in corso...\n");
        chiusura();
        exit(0);
    }
}


//Stampa la matrice
void stampa_matrice() {
  int dimmensione = MemCond->dimmensione;
  printf("\n");
  for(int i=0; i<dimmensione; i++) {
    for(int j=0; j<dimmensione; j++) {
      printf(" %c ", MemCond->matrice[i][j]);
      if (j < dimmensione - 1) 
        printf("|");
    }
    
    printf("\n");
    if(i < dimmensione -1) {
      for(int j =0; j<dimmensione; j++) {
        printf("---");
        if (j < dimmensione - 1) 
          printf("+");
      }
      printf("\n");
    }
  }
  printf("\n");
}



//Controllo se qualcuno ha vinto
void controlla_vincitore(){
  int dimmensione = MemCond->dimmensione;
  char simbolo[2] = {MemCond->simboli[0],MemCond->simboli[1]};
 
  for(int i= 0; i<2; i++){
        if(!MemCond->GAttivo[i]){
          MemCond->vincitore = 2; // un giocatore ha abbandonato la partita
        }
  }
  
  // Uso un for per controllare entrambi i giocatori
  for (int giocatore=0; giocatore<2; giocatore++) {
    
    // Faccio il controllo per righe e colonne
    for(int i=0; i<dimmensione; i++){
      int simbolorig=0;
      int simbolocol=0;
      
      for(int j=0; j<dimmensione; j++) {
        if(MemCond->matrice[i][j] == simbolo[giocatore]){
          simbolorig++;
          }
        if(MemCond->matrice[j][i] == simbolo[giocatore]){
          simbolocol++;
          }
      }
      if(simbolorig == dimmensione || simbolocol == dimmensione){
        MemCond->vincitore = giocatore;      
        }
      }
  
    // Verifico per diagonale
    int diagonaleLR = 0;
    int diagonaleRL = 0;
    for(int i=0; i<dimmensione; i++){
      if(MemCond->matrice[i][i] == simbolo[giocatore]){
        diagonaleLR++;
        }
      if(MemCond->matrice[i][dimmensione - i - 1] == simbolo[giocatore]){
        diagonaleRL++;
        }
     }
     
     if(diagonaleLR == dimmensione || diagonaleRL == dimmensione){
        MemCond->vincitore = giocatore;      
      }
  }
}


//Inizializzo la memoria
void InizializzaRisorse(int timeout, int dimmensione, char Giocatore1, char Giocatore2){

  MemCond->timeout = timeout;
  MemCond->dimmensione = dimmensione;
  MemCond->giocatoreattuale= 0;
  MemCond->vincitore= -1;
  MemCond->numeromosse = 0;
  MemCond->numeroturni = 0;  
  MemCond->maxmosse = dimmensione * dimmensione;
  MemCond->simboli[0] = Giocatore1;
  MemCond->simboli[1] = Giocatore2;
  MemCond->numerogioccatori = 0;
  MemCond->server = true;
  MemCond->SinglePlayer = false;
  MemCond->GAttivo[0] = true;
  MemCond->GAttivo[1] = true;
  
  //Inizializzo la matrice
  for(int i = 0; i < dimmensione; i++) {
    for(int j = 0; j < dimmensione; j++) {
      MemCond->matrice[i][j] = ' ';     
    }
  }
}



void MossaAutomatica(){
    int riga, colonna;
    int dimmensione = MemCond->dimmensione;

    srand(time(NULL)); // Inizializza il generatore di numeri casuali
    
    do {
        // Genera una riga e una colonna casuali
        riga = rand() % dimmensione;
        colonna = rand() % dimmensione;
        
        // Se la cella è già occupata, prova un'altra coppia di coordinate
    } while (MemCond->matrice[riga][colonna] != ' ');

    printf("E stat scelta la posizione con coordinate (%d, %d).\n", riga, colonna);
    
    MemCond->matrice[riga][colonna] = MemCond->simboli[1];
    MemCond->numeromosse++;
    MemCond->numeroturni++;
    MemCond->giocatoreattuale=0;

}

int main(int argc, char *argv[]) {
  if(argc < 4){
    printf("Uso: %s <timeout><Simbolo_Giocatore1><Simbolo_Giocatore2>\n", argv[0]);
    exit(1);
  }
  
  int timeout=atoi(argv[1]);
  char Giocatore1 = argv[2][0];
  char Giocatore2 = argv[3][0];
  
  int dimmensione; //Dimmensione default 3x3
  
  //Dettermino la dimmensione della matrice
  printf("\n Inserisci la dimmensione della marice su cui giocare (Dimmensione minima 3, massima %d): ", Max_Dim);
  scanf("%d", &dimmensione);
  if (dimmensione < 3 || dimmensione > Max_Dim) {
    printf("\n Dimmensione non valida. Uscita.\n");
    exit(1);
  }
  
  
  //Inizio la Partita
  shm_id = shmget(SHM_KEY, sizeof(MemoriaCondivisa), IPC_CREAT | 0640);
  if (shm_id <0){
    perror("shmget");
    exit(1);
  }
  
  MemCond = (MemoriaCondivisa *)shmat(shm_id, NULL, 0);
  if(MemCond == (void *)-1) {
    perror("shmat");
    exit(1);
  }
  
  sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0640);
  if (sem_id < 0) {
    perror("semget");
    exit(1);
  }
  
  semctl(sem_id, 0, SETVAL, 1); // Inizializzo il semaforo a 1
  signal(SIGINT, handle_sigint);
  InizializzaRisorse(timeout, dimmensione, Giocatore1, Giocatore2);
  
  // Stampo per la prima volta la matrice
  stampa_matrice();
  
  int contaMosse = MemCond->numeromosse;
  int contaTurni = MemCond->numeroturni;

  while (MemCond->vincitore == -1 && MemCond->numeromosse < MemCond->maxmosse) {
      
      sleep(0);
      lock_semaphore(sem_id, 0);

      // Controllo se è stata effettuata una nuova mossa
      if (MemCond->numeroturni != contaTurni) {
          stampa_matrice();  // Stampiamo subito la matrice aggiornata
          //
          if(MemCond->SinglePlayer){
            MossaAutomatica();
            stampa_matrice();
          }
          
          controlla_vincitore();  // Controlliamo se c'è un vincitore
          
          //Controlla se si sono collegati piu di due giocatori
          if(MemCond->numerogioccatori > 2){
              unlock_semaphore(sem_id, 0);
              break;
          }
          // Se il gioco è finito, usciamo dal loop
          if (MemCond->vincitore >= 0 || MemCond->numeromosse >= MemCond->maxmosse) {
              unlock_semaphore(sem_id, 0);
              break;
          }
          contaTurni = MemCond->numeroturni; // Aggiorna il contegio dei turni
          contaMosse = MemCond->numeromosse;  // Aggiorniamo il conteggio delle mosse
      }

      unlock_semaphore(sem_id, 0);
  }

  chiusura();
  return 0;
  
}

