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

#define SHM_KEY 1234
#define SEM_KEY 5678
#define Max_Dim 10

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
char NomeGioccatore[50];
int IdGiocatore = -1;

//Funzioni per semafori
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


// gestisce chiusura da parte del giocatore
void chiusura(){
  if (shmdt(MemCond) == -1) 
      perror("shmdt");
}



//Gestione Segnali
void handle_sigint(int sig) {
    static int CTRC = 0;
    CTRC++;
    
    if (CTRC == 1) {
        printf("\nPremi di nuovo CTRL-C per terminare la partita.\n");
    } else if (CTRC == 2) {
        printf("\nAbbandono Partita...\n");
        if(MemCond->giocatoreattuale == IdGiocatore){
          MemCond->GAttivo[IdGiocatore] = false;
          MemCond->numeroturni++;
          MemCond->giocatoreattuale = 1 - IdGiocatore;
          unlock_semaphore(sem_id, 0);
        } else{
          MemCond->GAttivo[IdGiocatore] = false;
        }
        chiusura();
        exit(0);
    }
}

void handle_alarm(int sig) {
    printf("\nTempo scaduto! Hai abbandonato la partita.\n");
    MemCond->giocatoreattuale = 1 - IdGiocatore;  // Passa il turno
    MemCond->GAttivo[IdGiocatore] = false;
    MemCond->numeroturni++;
    MemCond->giocatoreattuale = 1 - IdGiocatore;
    unlock_semaphore(sem_id, 0);
    chiusura();
    exit(0);
}


//Stampa Matrice Gioco
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



// Effettua Mossa
void mossa_giocatore() {
  int riga, collona;
  int dimmensioneM = MemCond->dimmensione;
  
  alarm(MemCond->timeout); // Imposto un allarme
  
  // Prendo le coordinate della matrice
  do{
    printf("%s, inserisci riga e colonna (0-%d): ", NomeGioccatore, dimmensioneM - 1);
    scanf("%d %d", &riga, &collona);
    if(riga>dimmensioneM-1 || collona >dimmensioneM-1){
      printf("\nPosizione fuori dalla matrice.\n");
    }
  } while (riga < 0 || riga >= dimmensioneM || collona < 0 || collona >= dimmensioneM);
  
  alarm(0); //Disattivo l'allarme
  //Controllo se il giocatore ha provato di imbrogliare
  if (MemCond->matrice[riga][collona]!= ' '){
    printf("\nPosizione gia occupata siccome hai provato di imbrogliare perdi il turno.\n");
  } 
  else {
    MemCond->matrice[riga][collona] = MemCond->simboli[IdGiocatore];
    MemCond->numeromosse++;
  }
  
}
  
int TFVincitore(){
    if (MemCond->vincitore == 2) {
        printf("Hai vinto %s! L'altro giocatore ha abbandonato la partita\n", NomeGioccatore);
        return 1;
    }else if (MemCond->vincitore == IdGiocatore) {
        printf("Complimenti %s! Hai vinto!\n", NomeGioccatore);
        return 1;
    } else if (MemCond->vincitore == -1 && MemCond->numeromosse == MemCond->maxmosse) {
        printf("Partita terminata in pareggio!\n");
        return 1;
    } else if (MemCond->vincitore == 1 - IdGiocatore){
        printf("Mi dispiace %s, hai perso!\n", NomeGioccatore);
        return 1;
    }
    return 0;
}

void GioccoAutomatico(){
  printf("Inizio della partita\n\n\n");
  
  while (MemCond->GAttivo[IdGiocatore]) {
        lock_semaphore(sem_id, 0);
        if(!MemCond->SinglePlayer){
          MemCond->SinglePlayer = true;
        }
        if(TFVincitore() == 1){
          unlock_semaphore(sem_id, 0);
          break;
          }
        // Controlla se il server è ancora attivo
        if (!MemCond->server) {
            printf("Il server si è spento. Il gioco è stato interrotto.\n");
            unlock_semaphore(sem_id, 0);
            break;
        }
        // Se è il turno del giocatore, esegui la mossa
        if (MemCond->giocatoreattuale == IdGiocatore) {
            stampa_matrice();
            mossa_giocatore();
            MemCond->numeroturni++;
            MemCond->giocatoreattuale = 1 - IdGiocatore;
            stampa_matrice();
        }

        unlock_semaphore(sem_id, 0);
        sleep(1);
    }
  exit(0);

}

int main(int argc, char *argv[]) {
  bool SinglePlayer = false;
  if (argc < 2) {
    printf("Uso: %s <nome_giocatore>\n", argv[0]);
    exit(1);
  } else if (argc == 2){
    strcpy(NomeGioccatore, argv[1]);
  } else if(argc == 3 && strcmp(argv[2], "*") == 0){
    strcpy(NomeGioccatore, argv[1]);
    SinglePlayer = true;
  }
  
  signal(SIGINT, handle_sigint);
  signal(SIGALRM, handle_alarm);
  
  shm_id = shmget(SHM_KEY, sizeof(MemCond), 0640);
  if(shm_id < 0) {
    perror("shmget");
    exit(1);    
  }

  MemCond = (MemoriaCondivisa *)shmat(shm_id, NULL, 0);
  if(MemCond == (void *)-1) {
    perror("shmat");
    exit(1);
  }
  
  sem_id = semget(SEM_KEY, 1, 0666);
  if (sem_id < 0) {
    perror("semget");
    exit(1);
  }
  
  IdGiocatore = MemCond->numerogioccatori;
  MemCond->numerogioccatori++;
  printf("Benvenuto %s, sei il %d gioccatore con simbolo %c.\n", NomeGioccatore, IdGiocatore +1 ,MemCond->simboli[IdGiocatore]);
  if(MemCond->timeout != 0){
    printf("Hai a disposizione %d secondi per effettuare una mossa oppure vera registrato come abbandono della partita.\n", MemCond->timeout);
  }
  
  //Controlla se il Giocatore vuole giocare da solo
  if(SinglePlayer){
    GioccoAutomatico();
  }else{
    printf("Attesa secondo gioccatore...\n");
  }
  
  while (MemCond->numerogioccatori == 1) {
    sleep(1);
  }
  printf("Inizio della partita\n\n\n");
  
  while (MemCond->GAttivo[IdGiocatore]) {
        lock_semaphore(sem_id, 0);
        
        if(TFVincitore() == 1){
          unlock_semaphore(sem_id, 0);
          break;
          }
        // Controlla se il server è ancora attivo
        if (!MemCond->server) {
            printf("Il server si è spento. Il gioco è stato interrotto.\n");
            unlock_semaphore(sem_id, 0);
            break;
        }
        // Se è il turno del giocatore, esegui la mossa
        if (MemCond->giocatoreattuale == IdGiocatore) {
            stampa_matrice();
            mossa_giocatore();
            MemCond->numeroturni++;
            MemCond->giocatoreattuale = 1 - IdGiocatore;
            stampa_matrice();
        }

        unlock_semaphore(sem_id, 0);
        sleep(1);
    }

  chiusura();
  return 0;
}
