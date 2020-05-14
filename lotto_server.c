/* 
 * File:   lotto_server.c
 * Author: Olgerti Xhanej
 *
 * Created on 13 dicembre 2019, 22:25
 */

/*
 * Struttura file utente:
 * [L/N] username
 * password
 * <giocata> [<timestamp> <vincite per giocate> / placeholder]
 */

/*
 * Struttura file estrazioni:
 * Estrazione <timestamp>
 * <ruota> <uscite>
 */

/*
 * Struttura file namefile_list:
 * namefile.txt
 */

/*
 * Struttura user bannati
 * <ip_client_bannato> <timestamp inizio ban>
 */

#define _POSIX_SOURCE
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h> 
#include <semaphore.h> 
#include <unistd.h>

//Definizione macro 
#define IP_BROADCAST "255.255.255.255"
#define ALREADY_LOGGED 'L'
#define NOT_LOGGED 'N'
#define NUM_COMMANDS 8
#define MAX_PIECES 20000 //Numero massimo argomenti passabili ad un comando 
#define MAX_BUF_SIZE 20000
#define MAX_SIZE_USR 100 //Numero massimo di caratteri username
#define MAX_SIZE_PW 100 //Numero massimo di caratteri username
#define MAX_FILENAME_SIZE 104
#define SESSION_ID_SIZE 10
#define RANDOM_VECTOR_SIZE 60
#define NUM_CITIES 11
#define NUM_GIOCATE 5
#define MAX_LINE 200
#define MAX_WIN_SEGNAPOSTO 100
#define THIRTY_MIN 1800 //Numero di secondi che compongono 30 minuti


//Definizione buffer e stringhe globali
char* commands_vect[8] = {"!help", "!signup", "!login", "!invia_giocata", "!vedi_giocate", "!vedi_estrazione", "!vedi_vincite", "!esci"};
char* city_vect[NUM_CITIES] = {"Bari", "Cagliari", "Firenze", "Genova", "Milano", "Napoli", "Palermo", "Roma", "Torino", "Venezia", "Nazionale"};
char* city_vect_minuscolo[NUM_CITIES] = {"bari", "cagliari", "firenze", "genova", "milano", "napoli", "palermo", "roma", "torino", "venezia", "nazionale"};
char* giocata_vect[NUM_GIOCATE] = {"estratto", "ambo", "terno", "quaterna", "cinquina"};
char random_vector[RANDOM_VECTOR_SIZE + 1] = "ABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyz1234567890";
char segnaposto_vincita[MAX_WIN_SEGNAPOSTO + 1] = "????????????????????????????????????????????????????????????????????????????????????????????????????";

/* SEZIONE STRUTTURE DATI*/

//Descrittore utente
struct user_des {
    char username[MAX_SIZE_USR];
    char password[MAX_SIZE_PW];
    char namefile[MAX_FILENAME_SIZE]; //Nome File registro associato 
    char session_id[SESSION_ID_SIZE+1]; 
    int status_login; //Status del login 0: se non si è ancora autenticato, 1 se l'utente associato si è autenticato
    int counter_wrong_login; //Contatore numero errato login da parte di un client
};


//Matrice che tiene conto dell'ultima estrazione effettuata
int matrix_extraction[11][5];

//Fattoriali comuni (da non stare a ricalcolare continuamente)
int common_factorial[11] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800};

//Vincite di base per tipologia di giocata
float num_win[5] = {11.23, 250, 4500, 120000, 6000000};

int kill(pid_t pid, int sig);

/*SEZIONE FUNZIONI DI UTILITA'*/
/*
 * Funzione di inizializzazione del server, si occupa di settare correttamente alcuni valori contenuti
 * nei file, i quali, a seguito di una chiusura non corretta del programma possono aver perso significato
 */
void init() {
    FILE* namefile_list_f; //Puntatore a file contenente lista dei file degli utenti registrati
    FILE* user_f; //Puntatore a file per ogni file utente
    char user_filename[MAX_FILENAME_SIZE]; 
    char* s; //Memorizzerà i valori di ritorno della fgets

    
    //printf("[DEBUG] init()\n");
    namefile_list_f = fopen("namefile_list.txt", "r"); //Apro file contenente lista dei file utenti
    
    //Gestione caso file non presente
    if(namefile_list_f == NULL){
        namefile_list_f = fopen("namefile_list.txt", "a");
        return;
    }

    s = fgets(user_filename, MAX_FILENAME_SIZE, namefile_list_f); //Ottengo filename da aprire
    if(s == NULL){ //Nessuna lettura effettuata
        fclose(namefile_list_f);
        return;
    }
    
    //Piccola correzione a fgets per togliere \n dalla stringa ottenuta
    user_filename[strlen(user_filename)-1] = '\0';
    
    
    //Apro i file utenti
    user_f = fopen(user_filename, "r+");
    
    //printf("[DEBUG] %s aperto\n", user_filename);

    //Se file utente esiste lo imposto come non loggato
    while(user_f != NULL) {
        fputc(NOT_LOGGED, user_f); //Imposto user come non loggato
        fclose(user_f);
        //printf("[DEBUG] %s chiuso\n", user_filename);

        //Ricomincio il ciclo passando al prossimo file utente da aprire
        s = fgets(user_filename, MAX_FILENAME_SIZE, namefile_list_f);
        if(s == NULL){
            fclose(namefile_list_f);
            return;
        }
            
        user_filename[strlen(user_filename)-1] = '\0';
        //printf("[DEBUG] %s\n", user_filename);
        user_f = fopen(user_filename, "r+");
        //printf("[DEBUG] %s aperto\n", user_filename);
    }
            
    fclose(user_f);        
    fclose(namefile_list_f);
    return;
}

/*
 * Funzione che genera un session id di 10 caratteri alfanumerici
 * ritorna:
 * 0 in caso di successo
 * -1 in caso di errore
 */
int generate_session_id(char* session_id) {
    int i=0;
    srand(time(NULL)); //Setto il seme per la generazione casuale 
    for(i=0; i<SESSION_ID_SIZE; i++){
        session_id[i] = random_vector[rand() % RANDOM_VECTOR_SIZE];
    }
 
    session_id[SESSION_ID_SIZE] = '\0';
    return 0;
}

/*
 * Funzione per gestione signup
 * ritorna
 * -1: se utente era già registrato (quindi esiste già un file con il suo username)
 * 0: se è stato salvato correttamente l'utente nel suo nuovo file registro 
 *
 */
int handle_signup(char* username, char* password, FILE* f) {
    //Provo ad aprire file relativo ad username
    FILE* file_users;
    char work[MAX_SIZE_USR];
    int ret = -1;
    memset(work, '\0', MAX_SIZE_USR); //Pulizia del buffer work
    
    //printf("[DEBUG] Inizio handle_signup: strlen_usr: %d\n", strlen(username));
    
    strncpy(work, username, strlen(username));   
    strcat(work, ".txt"); //Creo file utente dal suo nome utente aggiungendo .txt
    
    //printf("[DEBUG] username: %s, work: %s, password: %s\n", username, work, password);
    
    //printf("[DEBUG] Apertura file in lettura\n");
    
    f = fopen(work, "r"); //Apro file utente
    
    if(f == NULL) { //Nel caso il file non esista lo creo in questo momento (l'username inviato quindi è nuovo)      
        //printf("[DEBUG] Apertura file in scrittura\n");

        f = fopen(work, "a"); //L'apertura del file in append determina anche la creazione del file se questo non esiste
        fprintf(f, "N %s\n%s\n", username, password);
        ret = 0;
    }
    
   
    //Se ciò ha successo richiudo il file e restituisco 0
    //Altrimenti restituisco -1
    fclose(f);
    
    //Aggiorna file con lista file users
    file_users = fopen("namefile_list.txt", "a");
    fprintf(file_users, "%s\n", work);
    fclose(file_users);
    
    //printf("[DEBUG] Fine handle\n");
    
    return ret;
}

/*
 * Funzione per gestione login, comparazione e memorizzazione dei dati
 * ritorna:
 * -1 : se il login non ha avuto successo (password non corretta, username inesistente ...)
 * 0: se il login ha avuto successo 
 */
int handle_login(char* username, char* password, FILE* f) {
    char work[MAX_SIZE_USR]; //Buffer per memorizzare filename
    char username_mem[MAX_SIZE_USR]; //Buffer per memorizzare username del file registro
    char password_mem[MAX_SIZE_PW]; //Buffer per memorizzare password del file registro
    int ret = -1;
    char c;
    
    //Pulizia dei buffer di lavoro
    memset(work, '\0', MAX_SIZE_USR); 
    memset(username_mem, '\0', MAX_SIZE_USR); 
    memset(password_mem, '\0', MAX_SIZE_PW); 
      
    //printf("[DEBUG] Inizio handle_login: strlen_usr: %d\n", (int)strlen(username));
    strncpy(work, username, strlen(username));
    
    //Ottengo file utente
    strcat(work, ".txt"); 
    //printf("[DEBUG] username: %s, work: %s, password: %s\n", username, work, password);
    
    //printf("[DEBUG] Apertura file in lettura\n");
    f = fopen(work, "r+");
    
    if(f == NULL) { //Caso utente non si è ancora registrato (in quanto il file non è ancora stato creato)
        return -1;
    } else {       //Caso utente registrato
        
        c = getc(f); //Guardo primo carattere che identifica stato_login
                
        c = getc(f);
        fscanf(f, "%s", username_mem); //Leggo username da file
        fscanf(f, "%s", password_mem); //Leggo pw da file
        
        //Comparazione password
        if(strcmp(password_mem, password) == 0){
            fseek(f, 0, SEEK_SET);
            c = getc(f); //Guardo primo carattere che identifica stato_login
            
            //Se l'utente ha già effettuato il login non posso effettuare il login dello stesso utente da due client diversi contemporaneamente
            if(c == ALREADY_LOGGED){
                printf("[ERR] Utente già loggato\n");
                fclose(f);
                return -3; 
            }

            fseek(f, 0, SEEK_SET);
            fputc(ALREADY_LOGGED, f);
            ret = 0;
        }
        //Se le password (quella inviata e quella memorizzata nel file) non sono identiche
        else 
            ret = -2;
            
        //printf("[DEBUG] usr: %s, pw: %s, usr_file: %s, pw_file: %s\n", username, password, username_mem, password_mem);
        fclose(f);
        
        return ret;
    }
}

/*
 * Funzione che ritorna la posizione di stringa all'interno di vettore di stringhe vector 
 * ritorna.
 * -1: se non è stata trovata corrispondenza tra la stringa ed il vettore di stringhe
 * i >= 0: la posizione della stringa all'interno del vettore di stringhe
 */
int get_index_by_name(char* str, char** vector, int size) {
    int i=0;
    for(i=0; i<size; i++) { 
        if(strcmp(str, vector[i]) == 0)
            return i;
    }
    
    //Non è stata trovata alcuna uguaglianza
    return -1;
}

/*
 * Funzione che sposta il puntatore del file alla num_jumps riga
 * Importante: il FILE* f deve essere già aperto
 * ritorna -1: se il file termina prima della num_jumps riga
 * ritorna 0: se l'operazione è avvenuta con successo
 */
int jump_lines(FILE* fp, int num_jumps) {
    char c;
    
    //printf("[DEBUG] Inizio jump_lines: %d\n", num_jumps);
    //Gestione caso nullo
    if(num_jumps == 0)
        return 0;

    // Naviga il file alla ricerca dei newline 
    for (c = getc(fp); c != EOF && num_jumps > 0; c = getc(fp)) {    
        //printf("[DEBUG] %c", c);
        if (c == '\n') 
            num_jumps = num_jumps - 1;
        if(num_jumps == 0)
            break;
    }
            
    //printf("[DEBUG] Fine jump_lines: %d\n", num_jumps);
    
    //Gestione ritorno
    if(num_jumps != 0)
        return -1;
    else
        return 0;
}

/*
 * Funzione che conta le estrazioni effettuate dal file extractions.txt
 */
int count_extractions(FILE* fp) {
    char c;
    int count = 0;
    // Apre il file 
    fp = fopen("extractions.txt", "r"); 
    
    //Nel caso il file delle estrazioni non esista non sono state effettuate ancora estrazioni
    if(fp == NULL) {
        return 0;
    }

    // Naviga il file alla ricerca dei newline 
    for (c = getc(fp); c != EOF; c = getc(fp)) 
        if (c == '\n') 
            count = count + 1; 
    
    fclose(fp);
    
    //Ogni estrazione nel file occupa un numero fissato di righe (12)
    return count/12;
}

/*
 * Gestione copia risultati estrazioni su tutte le ruote
 */
void handle_vedi_estrazione_tot(char* n, FILE* fd, char* response) {
    int estrazioni = count_extractions(fd); //Numero totale di estrazioni effettuate
    int estrazione_n = atoi(n); //Vogliamo le ultime n estrazioni 
    int ret;
    int index = 0, i = 0;
    char line[100]; //Buffer di lavoro
    
    //Gestione caso con nessuna estrazione effettuata
    if(estrazioni == 0) { 
        strcpy(response, "ERROR nessuna_estrazione_effettuata");
        return;
    }

    
    //printf("[DEBUG] estrazioni: %d, estrazione_n: %d\n", estrazioni, estrazione_n);
    fd = fopen("extractions.txt", "r");
    
    //Mi sposto nell'estrazione n-esima
    ret = jump_lines(fd, (estrazioni*12) - (estrazione_n*12)); //Salta un numero di righe per posizionarsi all'estrazione voluta
    
    //Spostamento non ha avuto successo (es. file finito prima)
    if(ret == -1) {
        fclose(fd);
        //printf("[DEBUG] jump_lines fallita\n");
        strcpy(response, "ERROR giocate_richieste_non_effettuate");
        return;
    }
    
   //printf("[DEBUG] linee saltate\n");
    strcpy(response, "OK ");
    
    //Inizio copia dell'estrazione
    for(index = 0; index < estrazione_n; index++) {
        for(i = 0; i < 12; i++) { //Ogni estrazione è memorizzata su file in 12 righe
           fgets(line, 100, fd);
           //printf("[DEBUG] line: %s\n", line);
           strcat(response, line); 
        }        
    }
    
    fclose(fd);
}

/*
 * Funzione per la gestione del comando !exit
 * deve rendere significativi i file dell'utente che ha effettuato logout
 */
void handle_exit(char* session_id, char* namefile, int logged) {
    FILE* f;
    //printf("[DEBUG] status_login: %d\n", logged);

    //Se l'utente non è loggato allora non devo modificare alcun file utente
    if(logged == 0)
        return;
    
    //Gestione pulizia session_id corrente
    //printf("[DEBUG] pulizia session_id: %s\n", session_id);
    memset(session_id, 0, strlen(session_id));
    

    f = fopen(namefile, "r+");
    fputc(NOT_LOGGED, f); //Rendo non loggato l'utente che sta chiudendo la connessione
    fclose(f);
}

/*
 * Gestione copia risultati estrazione ruota specifica
 */
void handle_vedi_estrazione_ruota(char* n, char* ruota, FILE* fd, char* response) {
    int id = get_index_by_name(ruota, city_vect, NUM_CITIES); //Ottengo indice ruota all'interno del vettore contenente le ruote
    
    int estrazioni = count_extractions(fd); //Numero totale di estrazione effettuate
    int estrazione_n = atoi(n); //Vogliamo le ultime n estrazioni
    int index = 0; // i = 0;
    char line[100];
    int ret;
    
    //Ruota indicata dall'utente non esiste
    if(id == -1) {
        //Ripeto controllo in caso nome ruota sia minuscolo
        id = get_index_by_name(ruota, city_vect_minuscolo, NUM_CITIES);
        if(id == -1){
            strcpy(response, "ERROR ruota_non_esiste");
            return;
        }
    }
    
    //Non vi sono estrazioni effettuate
     if(estrazioni == 0) {
        strcpy(response, "ERROR nessuna_estrazione_effettuata");
        return;
    }
    
    //printf("[DEBUG] estrazioni: %d, estrazione_n: %d\n", estrazioni, estrazione_n);
    fd = fopen("extractions.txt", "r"); 
    ret = jump_lines(fd, (estrazioni*12) - (estrazione_n*12)); //Salta un numero di righe per posizionarsi all'estrazione voluta
    
    //Spostamento non ha avuto successo (es. file finito prima)    
    if(ret == -1) {
        fclose(fd);
        printf("[ERR] jump_lines fallita\n");
        strcpy(response, "ERROR giocate_richieste_non_effettuate");
        return;
    }
    
    //printf("[DEBUG] linee saltate\n");
    strcpy(response, "OK ");

    //Mi preparo alla copia dell'estrazione
    for(index = 0; index < estrazione_n; index++) {

        //Copio prima riga estrazione con timestamp
        fgets(line, 100, fd);
        ret = jump_lines(fd, id);
        //printf("[DEBUG] line: %s\n", line);
        strcat(response, line);

        //Copio riga della ruota desiderata
        fgets(line, 100, fd);
        ret = jump_lines(fd, (10-id));
        //printf("[DEBUG] line: %s\n", line);
        strcat(response, line); 
           
    }
    
    fclose(fd);
    return;
}

/*
 * Funzione che si occupa di salvare su file utente la giocata inviata dal relativo utente
 * ritorna:
 * 0: in caso il salvataggio della giocata sia andato a buon fine
 * 1: altrimenti
 */
int handle_invia_giocata(char** response_piece, int num_pieces, char* namefile) {
    int i = 1; //Variabile per scansionare la giocata ricevuta
    FILE* f;
    f = fopen(namefile, "a");
    
    //printf("[DEBUG] Inizio handle invia giocata\n");
    
    //ret = fseek(f, 0, SEEK_END);
    
    //Se avviene un errore nell'apertura segnalo l'errore al client
    if(f == NULL) {
        //printf("[DEBUG] Errore nella apertura file\n");
        return -1;
    } else {
        //printf("[DEBUG] fseek andata a buon fine\n");
    }
    
    //Le giocate inviate devono ancora essere estratte, pertanto sono di tipo '1' per il comando !vedi_giocate
    fprintf(f, "1");

    //Copia della giocata nel file
    for(; i < num_pieces-1; i++) {
        //printf("[DEBUG] response_piece[%d] : %s\n", i, response_piece[i]);
        fprintf(f, " %s", response_piece[i]);
    }
    
    //Devo memorizzare abbastanza spazio per andare successivamente a calcolare e memorizzare la vincita
    fprintf(f, " %s \n", segnaposto_vincita);
    fclose(f);
    return 0;
}

/*
 * Funzione che ha il compito di gestire le richieste dei client quando mandano il comando !vedi_giocata
 * Serializza i dati delle giocate richieste dal parametro type e le prepara nel buffer da inviare (response)
 */
void handle_vedi_giocata(char* response, int type, char* namefile) {
    FILE* fp; //Relativo a file utente
    char c; //Carattere per scansionare il file
    char line[MAX_LINE]; //Linea di testo da leggere nel file
    int ret = 0;
    
    fp = fopen(namefile, "r");
    
    
    //printf("[DEBUG] Inizio handle vedi_giocata, type: %d, namefile: %s\n", type, namefile);
    
    //Controllo se apertura file è andata a buon fine
    if(fp == NULL) {
        //printf("[DEBUG] Errore nella apertura file\n");
        strcpy(response, "ERROR file_not_opened");
        return;
    } else {
        //printf("[DEBUG] Apertura file andata a buon fine\n");
    }
    
    //Sposto il puntatore della read alle giocate
    ret = jump_lines(fp, 2);
    if(ret == -1) {
        //printf("[DEBUG] Jump-lines fallita\n");
        printf("[ERR] File %s terminato in modo prematuro. chiusura del file", namefile);
        fclose(fp);
        strcpy(response, "ERROR file_utente_corrotto");
        return;
    }
    
    strcpy(response, "OK ");
    
    //Scansiona il file utente per copiare le giocate nel buffer di risposta
    for (c = getc(fp); c != EOF; c = getc(fp)) {
        //printf("[DEBUG] %c\n", c);
        
        //controllo il primo parametro: se rispetta il tipo richiesto dal client avviene la copia 
        if((type == 0 && c == '0') || (type == 1 && c == '1')) {
           c = getc(fp);
           fgets(line, MAX_LINE, fp); //Leggo l'intera riga
           //printf("[DEBUG] type: %d, line: %s\n", type, line);
           strcat(response, line);
        } else {
            ret = jump_lines(fp, 1); //Passo alla prossima riga
            if(ret == -1){
                //printf("[DEBUG] Jump-lines fallita\n");
            }
        }
    }
    
    //printf("[DEBUG] copia finita\n");
    fclose(fp);
}

/* 
 * Interpreta la stringa inviata come un comando, controllandone il formato
 * ritorna:
 * -1 -> Comando non trovato
 * i>=0 -> Comando i-esimo trovato
 */

int find_command(char* command) {
    int index = 0;
    
    //Ricerca all'interno del vettore commands_vect il comando passato
    while(index < NUM_COMMANDS) {
        if(strcmp(command, commands_vect[index]) == 0)
            return index + 1;
        index++;
    }
    
    return -1;    
}

/*
 * Funzione che spezza comando digitato in tutti i suoi componenti per esaminarli successivamente. 
 * Conta inoltre il numero di parametri.
 * ritorna: 
 * il numero di "pezzi" ottenuti
 * -1 -> in caso di errore (numero argomenti passati supera la dimensione del vettore di stringhe command_piece
 * Inoltre questa funzione ha effetti collaterali -> command_piece conterrà i vari comandi inseriti in buffer separati uno ad uno
 */

int break_command_in_pieces(char* buffer, char** command_piece, int MAX) {
    int num_pieces = 0;
    int index = 0; 

    char work[100];
    int ret_sscanf = 0;


    //Scorro tutto il buffer ricevuto fino alla fine
    while(ret_sscanf != EOF){
        //Prendo una stringa (terminata da spazi o \n o \t o \0)
        ret_sscanf = sscanf(&buffer[index], "%s", work);
        //printf("[DEBUG] Work: %s\n", work);

        //Copio la stringa nella locazione relativa del vettore
        strcpy(command_piece[num_pieces], work);
        index += strlen(work) + 1;
        num_pieces++;
        //printf("[DEBUG] command_piece[%d]: %s\n", num_pieces-1, command_piece[num_pieces-1]);
    }

    return num_pieces - 1;
}



/*
 * Funzione che gestisce le estrazioni ogni 'period' secondi
 */
void make_extraction(FILE* f, time_t cur_timestamp) {
    int i=0, j=0, k=0; 
    int num_rand; //Numero estratto casualmente
    struct tm tm = *localtime(&cur_timestamp); //Struttura per gestione stampa timestamp con formato specifico
    srand(time(NULL)); //Imposto seme di generazione numeri casuali
 
    f = fopen("extractions.txt", "a");
    
    //Stampa formato estrazione in base al formato dei minuti (se < 10 devo aggiungere uno 0)
    if(tm.tm_min >= 10)    
        fprintf(f, "Estrazione del %d-%d-%d ore %d:%d \n", tm.tm_mday,  tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
    else
        fprintf(f, "Estrazione del %d-%d-%d ore %d:0%d \n", tm.tm_mday,  tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
    

    //Scorro tutte le ruote
    for(i=0; i<NUM_CITIES; i++) {
        fprintf(f, "%s ", city_vect[i]);

        //Estraggo i 5 numeri per ruota
        for(j=0; j < 5; j++) {
               
            //Check per evitare doppioni dal numero estratto
            while(1) {
                num_rand = (rand() % 90) + 1;
                for(k = 0; k < j; k++) {
                    if(matrix_extraction[i][k] == num_rand)
                        break;
                }
                
                //Se non ci sono doppioni
                if(k == j)    
                    break;
            }
            
            matrix_extraction[i][j] = num_rand; //Aggiorno la matrice dell'ultima estrazione
            fprintf(f, "%d ", num_rand);
        }

        fprintf(f, "\n");
    }
    
    fclose(f);
}

/*
 * Gestisce client che sbaglia a loggarsi per 3 volte di fila.
 * Comporta il ban del client per 30 minuti
 */
void handle_wrong_login(struct sockaddr_in client_addr, char* response){
    FILE* banned_f;
    char ip_presentation_format[INET_ADDRSTRLEN];
    
    //Recupero IP address client
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_presentation_format, INET_ADDRSTRLEN);
    
    //printf("[DEBUG] Ip_client: %s\n", ip_presentation_format);
    
    //Scrivo su file IP address e timestamp
    banned_f = fopen("banned_users.txt", "a");
    fprintf(banned_f, "%s %d\n", ip_presentation_format, (int)time(NULL));

    //Mando messaggio !exit a client
    strcpy(response, "!esci 0");
    
    fclose(banned_f);
}

/*
 * Controlla se num è contenuto nel vettore vect, di dimensione dim (a differenza della get_index_by_name questa funzione lavora con gli interi)
 * ritorna:
 * -1: se num non è presente
 * 0: altrimenti
 */
int is_present(int num, int* vect, int dim) {
    int index;
    for(index = 0; index < dim; index++) {
        if(num == vect[index]) {
            //printf("[DEBUG] Num_trovato: %d\n", num);
            return 0;
        }
    }
    
    return -1;
}


/*
 * Funzione che per ogni estrazione effettuata ha il compito di:
 * 1-> Calcolare la vincita della schedina giocata per ogni utente, per ogni giocata attiva 
 * 2-> Rendere non più attive le giocate prese in considerazone
 */
void update_wins() {
    FILE* namefile_f; //Puntatore a file con i namefile dei vari file utenti
    FILE* user_f; //

    char namefile[MAX_FILENAME_SIZE]; //Buffer per contenere il nome dei file
    char work[100]; //Buffer di lavoro
    char result[50]; 
    int numbers[10]; //Contiene i numeri giocati
    int num_numbers; //Contiene il 'numero di numeri' contenuti in una giocata

    float bets[5]; //Contiene l'importo giocato per ogni tipologia

    int ruote_attive[11]; //Flag per le ruote indicate nella giocata
    int num_ruote_attive = 0; //Numero ruote indicate nella giocata

    int id; //Id per ruota all'interno del vettore city_vect_minuscolo
    int flag_i = 0; //Flag settato se è stato trovato campo -i
    int bet_index = 0;
    char c;
    float wins[5]; //Vincite totali per tipologia di giocata

    int index = 0;
    int index_2 = 0;
    int index_3 = 0;
    int counter = 0; //Contatore di numeri estratti corrispondenti con numeri della giocata
    
    int ret_fscanf; //Gestisce intero ritornato dalla fscanf
    
    //Pulizia buffer di lavoro
    memset(ruote_attive, 0, 11*sizeof(int));
    memset(wins, 0, 5*sizeof(float));
    
    //printf("[DEBUG] Inizio Update_win\n");

    //Apri file namefile_list ed ottieni nomi file registro per ogni utente
    namefile_f = fopen("namefile_list.txt", "r");
    
    //Gestione apertura errata namefile_list.txt
    if(namefile_f == NULL) {
        printf("[ERR] Impossibile aprire file namefile_list.txt\n");
        return;
    } 
    
    while(1) {

        ret_fscanf = fscanf(namefile_f, "%s", namefile);
        //printf("[DEBUG] Namefile: %s\n", namefile);
        
        //Gestione letture errate del namefile
        if(strcmp(namefile, "") == 0)
            break;
        if(ret_fscanf == EOF)
            break;
        
        user_f = fopen(namefile, "r+");
        
        if(user_f == NULL)
            printf("[ERR] Impossibile aprire file %s\n", namefile);
        
        //Salta prime 2 righe del file utente (username e password)
        jump_lines(user_f, 2);
        num_numbers = 0;
        
        //Ricerca giocate attive
        while(1) {
            c = getc(user_f);
            //printf("[DEBUG] c: %c\n", c);
  
            if(c == '0')
                jump_lines(user_f, 1); //Se trovo giocata passata la salto
            else if(c == EOF)
                break;
            else {
                fseek(user_f, -1, SEEK_CUR); //Una volta trovata una giocata attivo la rendo 'passata'
                fprintf(user_f, "0");
                break;
            }   
        }

        if(c != EOF) {
            while(1) {
                //Ricerca -r
                
                fscanf(user_f, "%s", work);
                //printf("[DEBUG] work: %s\n", work);


                if(strcmp(work, "-r") == 0 || strcmp(work, "-n") == 0)
                    continue;

                if(strcmp(work, "-i") == 0) {
                    flag_i = 1;
                    continue;
                }


                //Setto ruote trovate
                //Caso con ruota 'tutte'
                if(strcmp(work, "tutte") == 0) {
                    int i;
                    //printf("[DEBUG] tutto le ruote attive a 1\n");
                    num_ruote_attive = 11;
                    for(i=0; i<11; i++)
                        ruote_attive[i] = 1;
                    
                    continue;
                }

                //Caso con ruota specifica
                id = get_index_by_name(work, city_vect_minuscolo, NUM_CITIES);
                if(id != -1) {
                    //printf("[DEBUG] ruote_attive[%d]: 1\n", id);
                    ruote_attive[id] = 1;
                    num_ruote_attive++;
                    continue;
                }


                if(strcmp(work, "-n") == 0)
                    continue;

                //Estrazione numeri giocati e memorizzazione in variabile numbers
                if(flag_i == 0) {
                    numbers[num_numbers++] = atoi(work);
                    //printf("[DEBUG] numbers[%d]: %d\n",num_numbers - 1,  numbers[num_numbers - 1]);
                    continue;
                }

                //Estrazione importi giocati
                bets[bet_index++] = atof(work);
                //printf("[DEBUG] bet[%d]: %f\n", bet_index - 1, bets[bet_index - 1]);

                //Controllo fine riga
                fseek(user_f, 1, SEEK_CUR);
                c = getc(user_f);
                fseek(user_f, -1, SEEK_CUR);
                if(c != '?')                
                    continue;
                
                
                //Dopo aver preso tutti i dati della giocata inizio a calcolare la vincita
                strcpy(result, "-v ");
                //printf("[DEBUG] num_ruote: %d, ruote_attive[0]: %d, ruote_attive[1]: %d\n", num_ruote_attive, ruote_attive[0], ruote_attive[1]);

                //Calcolo vincita per tipo
                for(index_3 = 0; index_3 < 5; index_3++) { //Per ogni tipo di giocata

                    if(bets[index_3] != 0) { //Se la giocata non è nulla
                        
                        for(index_2 = 0; index_2 < NUM_CITIES; index_2++) { //Per ogni ruota giocata

                            counter = 0; //Contatore di numeri 'azzeccati'
                            
                            if(ruote_attive[index_2] == 1) { //Se la ruota in questione è stata considerata nella scommessa
                                for(index = 0; index < num_numbers; index++) { //Per ogni numero giocato
                                    if(is_present(numbers[index], matrix_extraction[index_2], 5) != -1) { //Controllo se il numero giocato è presente nell'estrazione della ruota scelta
                                        //printf("[DEBUG] Uguaglianza trovata. ruota: %s, numero: %d\n", city_vect[index_2], numbers[index]);
                                        counter++;
                                    }
                                }
                                
                                //printf("[DEBUG] tipo_giocata: %s, counter: %d, ruota: %s\n", giocata_vect[index_3], counter, city_vect[index_2]);
                                if(counter >= index_3 + 1) { //Se sono arrivato alla giocata aggiorno wins
                                    if(index_3 == 0)
                                        counter--;

                                    /*
                                    *   Formula utilizzata per il calcolo della vincita per tipo di giocata i-esima (index_3: 0->ESTRATTO, 1->AMBO...):
                                    *
                                    *     (!num_trovati/!numeri_per_vincita)*somma_vincita_i-esima
                                    *     ------------------------------------------------------------------
                                    *     (!num_giocati/!(num_giocati - numeri_per_vincita))*num_ruote_attive 
                                    *
                                    */


                                    wins[index_3] += (((common_factorial[counter]/common_factorial[index_3 + 1]))*num_win[index_3])/(((common_factorial[num_numbers]/common_factorial[num_numbers - index_3 -1]))*num_ruote_attive);
                                    //printf("[DEBUG] numeratore: !counter: %d, !(index_3 + 1): %d\n", common_factorial[counter], common_factorial[index_3 + 1]);
                                    //printf("[DEBUG] denominatore: !num_numbers: %d, !(num_numbers - index_3 -1): %d, ruote_attive: %d", common_factorial[num_numbers], common_factorial[num_numbers - index_3 - 1], num_ruote_attive);
                                    //printf("[DEBUG] wins_corrente: %f\n", wins[index_3]);
                                }
                            }
                        }
                    }

                    //Copio la vincita nel file
                    sprintf(work, "%f ", wins[index_3]);
                    //printf("[DEBUG] %d) Vincita: %s\n", index_3+1, work);
                    strcat(result, work);
                }

                //Copio timestamp nel buffer
                sprintf(work, "-t %d ", (int)time(NULL));
                strcat(result, work);

                //Copia stringa
                fprintf(user_f, "%s", result);
                //printf("[DEBUG] result: %s\n", result);

                //Salto alla prossima linea
                jump_lines(user_f, 1);
                

                //Pulizia delle variabili di lavoro e reset dei flag
                memset(ruote_attive, 0, 11*sizeof(int));
                memset(wins, 0, 5*sizeof(float));
                flag_i = 0;
                bet_index = 0;
                num_numbers = 0;
                num_ruote_attive = 0;
                

                //Controllo se ho raggiunto la fine del file
                c = getc(user_f);
                //printf("[DEBUG] c: %c\n", c);
                if(c == EOF || c == '\0' || c == ' ')
                    break;
                else {
                    fseek(user_f, -1, SEEK_CUR);
                    fprintf(user_f, "0");
                }
                    
            }
        }
        
        fclose(user_f);
        work[0] = '\0';
    }
    
    fclose(namefile_f);
    
}




/*
 * Controlla se un indirizzo ip di un client appartiene alla lista degli ip bannati (per superamento numero tentativi di login)
 * ritorna:
 * -1: se l'ip del client risulta bannato
 * 0: altrimenti
 */
int check_banned_client(struct sockaddr_in client_addr){
    FILE* banned_f;
    char ip_client[INET_ADDRSTRLEN];
    int current_timestamp = time(NULL);
    int recorded_timestamp; //Timestamp registrato nel file banned_users
    char ip_recorded[INET_ADDRSTRLEN];
    int flag = 0; //Settato a -1 se il client corrente è stato bannato e deve ancora scontare il periodo di ban  
    
    //Recupero IP address client
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_client, INET_ADDRSTRLEN);
    
    //printf("[DEBUG] Ip_client: %s\n", ip_client);
    
    banned_f = fopen("banned_users.txt", "r");
    
    if(banned_f == NULL) { //Controllo se file non esiste
        //printf("[DEBUG] File banned_users.txt non esiste\n");
        return 0;
    }
    
    //Scansiono il file banned_users.txt
    while(fscanf(banned_f, "%s %d", ip_recorded, &recorded_timestamp) != EOF){
        //printf("[DEBUG] Ip: %s, timestamp: %d, cur_timestamp: %d\n", ip_recorded, recorded_timestamp, current_timestamp);
        
        //Cerco corrispondenza tra ip del client ed ip registrato
        if(strcmp(ip_recorded, ip_client) != 0)
            continue;
        if(current_timestamp - recorded_timestamp < THIRTY_MIN){ //Controllo se sono passati 30 minuti dal timestamp registrato
            //printf("[DEBUG] Ban attivo trovato\n");
            flag = -1;
            break;
        }
    }
    
    return flag;
}

/*
 * Funzione che controlla se il session id ricevuto coincide con quello dell'utente memorizzato
 * ritorna:
 * -1: se i due session_id non coincidono
 * 0: altrimenti
 */
int check_session_id(char* session_id_received, char* session_id) {
    //printf("[DEBUG] session_id_rcv: %s, session_id: %s\n", session_id_received, session_id);
    if(strcmp(session_id_received, session_id) == 0)
        return 0;
    else
        return -1;    
}

int main(int argc, char** argv) {
    FILE *file_reg_pointer = NULL, *file_extractions_pointer = NULL; //Puntatore a file di registro
    int socket_des, client_socket_des; //Descrittori dei socket del client e del server
    int ret; //Variabile che memorizza il ritorno delle varie primitive
    unsigned int len; //Memorizza la lunghezza della stringa da mandare
    int period = 300; //Valore default periodo estrazione
    char buffer[MAX_BUF_SIZE]; //Buffer per ricezione comandi e funzione break_into_pieces
    char entire_command[MAX_BUF_SIZE]; //Buffer per contenere comando "intero" (con spazi inclusi
    char response[MAX_BUF_SIZE]; //Buffer per memorizzare la risposta al client
    char ip_client[INET_ADDRSTRLEN]; 
    int ret_scanf, n; //Variabili utili a determinare se un numero è intero o meno
    int i; //Variabile per inizializzazione vettore di stringhe
    
    //char* command_piece[MAX_PIECES]; //Vettore di stringhe contenente i vari argomenti per ogni comando;
    int num_pieces; //Conterrà il numero di parametri del comando + 1
    int command_id; //Id del comando ricevuto
    struct sockaddr_in my_addr, client_addr;
    int n_extr; 
    time_t cur_timestamp;
    int ret_check_banned_list; //Ritorno per funzione check_banned_list() per controllo eventuale ip bannato
    pid_t pid;
    uint32_t port_number;
    uint16_t len_server_format; //Variabile destinata a contentere lunghezza msg in formato server
    struct user_des current_user_des; //Descrittore di utente, serve per la gestione del session_id

    //Inizializzo session_id del client che si interfaccia al server
    current_user_des.session_id[0] =  '\0';
    
    //Gestione array di stringhe con prima inizializzazione   
    char** command_piece = (char**)malloc(sizeof(char*) * MAX_PIECES);
    for(i = 0 ; i < MAX_PIECES ; i++)
         command_piece[i] = (char*)malloc(sizeof(char) * 100);

    //Controllo parametri passati all'avvio del server
    if(argc < 2 || argc > 4) {
        printf("[ERR] Numero argomenti passati invalido.\nIl formato supportato è: ./lotto_server <porta> [<periodo>] \n");
        exit(-1);
    } else if(argc == 3) {

        //Controllo interezza periodo inserito
        ret_scanf = sscanf(argv[2], "%d%n", &period, &n);
        
        if(ret_scanf != 1 || n != strlen(argv[2])){
            printf("[ERR] Valore periodo inserito non e' un numero intero\n");
            exit(-1);        
        }
        
        //Controllo positività periodo
        if(period <= 0) {
            printf("[ERR] Parametro period invalido. Inserire un valore >=1\n");
            exit(-1);
        }

        period = period*60;
        //printf("[INFO] period: %d\n", period); //DEBUG
    } 
    
    //Controllo interezza numero di porta inserito
    ret_scanf = sscanf(argv[1], "%d%n", &port_number, &n);
    
    if(ret_scanf != 1 || n != strlen(argv[1])){
        printf("[ERR] Valore numero di porta inserito non e' un numero intero\n");
        exit(-1);        
    }  
    
    //Controllo validità numero di porta
    if(port_number < 1024 || port_number > 65535) {
            printf("[ERR] Numero porta indicato non permesso, inserire numero di porta nel range [1024, 65535]\nIl formato supportato è: ./lotto_server <porta> [<periodo>] \n");
            exit(-1);
    }
    //printf("[INFO] port: %d\n", port_number); //DEBUG
    
    init();
    
    pid = fork();
    
    //Processo che gestisce le estrazioni
    if(pid == 0) {
        while(1) {
            //printf("[DEBUG] period: %d, timestamp: %d, padre: %d\n", period, (int)cur_timestamp, getppid());
            sleep(period);

            ret = kill(getppid(), 0);
            if(ret == -1){
                //printf("[DEBUG] Uscita\n");
                exit(-1);
            }

            
            cur_timestamp = time(NULL);
            make_extraction(file_extractions_pointer, cur_timestamp);
            update_wins();
            n_extr = count_extractions(file_extractions_pointer);
            printf("[INFO] Estrazione effettuata: %d\n", n_extr);
        }

        exit(-1);        
    } else if (pid == -1) {
        perror("[ERR] Fork non riuscita: ");
        exit(-1);

    //Processo che ascolta/gestisce le richieste dei client    
    } else {

        //Creazione struttura dati socket
        socket_des = socket(AF_INET, SOCK_STREAM, 0);

        if(socket_des == -1)
            exit(-1);

        //Creazione indirizzo server
        memset(&my_addr, 0, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(port_number);
        inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

        fflush(stdout);

        //Binding dell'indirizzo appena configurato al socket creato
        ret = bind(socket_des, (struct sockaddr*)&my_addr, sizeof(my_addr));

        if(ret == -1) {
            perror("[ERR] Bind non riuscita: ");
            exit(-1);
        }

        //Processo entra in ascolto delle richieste da parte di client
        ret = listen(socket_des, 10);

        if(ret == -1) {
            perror("[ERR] Listen non riuscita: ");
            exit(-1);
        }


        printf("[INFO] Creazione Socket di Ascolto, port: %d \n", (int)port_number);
        while(1) {

            len = sizeof(client_addr);

            //Processo accetta richiesta di connessione da parte di un client
            client_socket_des = accept(socket_des, (struct sockaddr*)&client_addr, &len);
            inet_ntop(AF_INET, &client_addr.sin_addr, ip_client, INET_ADDRSTRLEN); 
            
         
            printf("[INFO] Connessione TCP inizializzata con client. IP address client: %s\n", ip_client);
            pid = fork();

            //Processo figlio che processa le richieste del nuovo client
            if(pid == 0){

                //Chiusura socket per ascolto
                close(socket_des);
                current_user_des.status_login = 0; //Imposto stato login a non ancora loggato
                current_user_des.counter_wrong_login = 0; //Imposto contatore login errati 
                
                //Controllo se il client non è stato bannato
                ret_check_banned_list = check_banned_client(client_addr);
                
                if(ret_check_banned_list == -1) //Se utente è stato bannato
                    strcpy(buffer, "NO");
                else
                    strcpy(buffer, "OK");
                
                //Send dimensione messaggio di 'benvenuto'
                len = strlen(buffer) + 1;
                len_server_format = htons(len);

                //printf("[DEBUG] response: %s, len: %d\n", response, len);
                ret = send(client_socket_des, (void*)&len_server_format, sizeof(uint16_t), 0);
                if(ret == -1) {
                    perror("[ERR] Send dimensione non riuscita: ");
                    exit(-1);
                }

                //Send messaggio di 'benvenuto'
                ret = send(client_socket_des, (void*)buffer, len, 0);
                if(ret == -1) {
                    perror("[ERR] Send risposta non riuscita: ");
                    exit(-1);
                }
                
                //Chiusura connessione in caso il client è ancora bannato
                if(ret_check_banned_list == -1){
                    printf("[INFO] Client bannato rilevato: chiusura connessione TCP\n");
                    close(client_socket_des);
                    exit(-1);
                }
                
                
                while(1) {
                    //Pulizia buffer di lavoro
                    memset(buffer, 0, 1024);
                    memset(entire_command, 0, 1024);
                    memset(response, 0, 1024);
                    len = 0;
                    
                    //Ricevo dimensione comando dell'utente
                    ret = recv(client_socket_des, (void*)&len_server_format, sizeof(uint16_t), 0);
                    if(ret == -1) {
                        perror("[ERR] Recv dimensione non riuscita: ");
                        exit(-1);

                    //Controllo se client ha chiuso la connessione
                    } else if(ret == 0){
                        handle_exit(current_user_des.session_id, current_user_des.namefile, current_user_des.status_login);
                        printf("[ERR] Chiusura connessione TCP per errore del client\n");
                        
                        //Chiudo connessione con client in stato di errore
                        close(client_socket_des);
                        exit(-1);
                    }


                    len = ntohs(len_server_format);
                    //printf("[DEBUG] len: %d\n", len);

                    //Ricevo comando dell'utente
                    ret = recv(client_socket_des, (void*)buffer, len, 0);
                    if(ret == -1) {
                        perror("[ERR] Recv comando non riuscita: ");
                        exit(-1);

                    //Controllo se client ha chiuso la connessione
                    } else if(ret == 0){
                        handle_exit(current_user_des.session_id, current_user_des.namefile, current_user_des.status_login);
                        printf("[ERR] Chiusura connessione TCP per errore del client\n");
                        
                        //Chiudo connessione con client in stato di errore
                        close(client_socket_des);
                        exit(-1);
                    }
                    

                    //printf("[DEBUG] Comando ricevuto: %s\n", buffer);
                    strncpy(entire_command, buffer, strlen(buffer));
                    //printf("[DEBUG] Comando ricevuto: %s\n", entire_command);


                    /** Processazione del comando ricevuto **/

                    //Deserializzo il comando e lo memorizzo nel vettore di stringhe command_piece
                    num_pieces = break_command_in_pieces(buffer, command_piece, MAX_PIECES);
                    //printf("[DEBUG] Comando ricevuto: %s, %s\n", buffer, entire_command);

                    //Check del comando inviato dal client 
                    command_id = find_command(command_piece[0]);
                    //printf("[DEBUG] command_id: %d\n", command_id);

                    //Gestione comando ricevuto
                    switch(command_id) {
                        case 2: //Signup
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            ret = handle_signup(command_piece[1], command_piece[2], file_reg_pointer);

                            //Gestione errore signup
                            if(ret == -1) {
                                //printf("[DEBUG] Utente già registrato\n");
                                strcpy(response, "ERROR username_already_registered"); 
                            } else {
                                //printf("[DEBUG] Nuova registrazione avvenuta\n");
                                strcpy(response, "OK"); 
                            }
                            break;
                        case 3: //login
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            ret = handle_login(command_piece[1], command_piece[2], file_reg_pointer);

                            //Gestione errore login (notare che i gli errori di username e password non sono differenziati, ciò rende il client ignaro della natura dell'errore)
                            if(ret == -1) {
                                //printf("[DEBUG] Utente non registrato\n");
                                strcpy(response, "ERROR wrong_credentials");
                                current_user_des.counter_wrong_login++; //Memorizzo errore login
                                
                                //Se ho effettuato 3 errori di fila memorizzo ip
                                if(current_user_des.counter_wrong_login == 3)
                                    handle_wrong_login(client_addr, response);
                            } else if(ret == -2) {
                                //printf("[DEBUG] Password inserita non è corretta\n");
                                strcpy(response, "ERROR wrong_credentials");
                                current_user_des.counter_wrong_login++; //Memorizzo errore login
                                
                                //Se ho effettuato 3 errori di fila memorizzo ip
                                if(current_user_des.counter_wrong_login == 3)
                                    handle_wrong_login(client_addr, response);
                            
                            //Gestione caso utente già loggato
                            } else if(ret == -3) {
                                //printf("[DEBUG] Utente già loggato\n");
                                strcpy(response, "ERROR user_already_logged_in");

                            } else {
                                strcpy(current_user_des.namefile, command_piece[1]);
                                strcat(current_user_des.namefile, ".txt");
                                //printf("[DEBUG] Login effettuato con successo, namefile: %s\n", current_user_des.namefile);
                                current_user_des.status_login = 1; //Imposto stato login a 1
                                
                                /* Gestione session id */
                                generate_session_id(current_user_des.session_id);
                                //printf("[DEBUG] session_id: %s\n", current_user_des.session_id);

                                strcpy(response, "OK ");
                                strcat(response, current_user_des.session_id);
                            }
                            break;
                        case 4: //invia_giocata
                            
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            
                            //Controllo session id
                            ret = check_session_id(command_piece[num_pieces-1], current_user_des.session_id);                            
                            if(ret == -1) {
                                strcpy(response, "ERROR wrong_session_id");
                                break;
                            }
                            

                            //printf("[DEBUG] sto per entrare in handle invia giocata\n");
                            
                            //Copia informazioni giocata su file utente
                            handle_invia_giocata(command_piece, num_pieces, current_user_des.namefile);
                            strcpy(response, "OK");
                            break;
                            
                        case 5: //vedi_giocate
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            
                            //Controllo session_id
                            if(num_pieces == 3)
                                ret = check_session_id(command_piece[2], current_user_des.session_id);
                            else {
                                printf("[ERR] argomenti per la vedi_giocate invalido");
                                strcpy(response, "ERROR wrong_number_parameter");
                                break;
                            }
                            
                            if(ret == -1) {
                                strcpy(response, "ERROR wrong_session_id"); 
                                break;
                            }
                            
                            //Gestione messaggio di risposta al comando vedi_giocate
                            handle_vedi_giocata(response, atoi(command_piece[1]), current_user_des.namefile);
                            break;
                                
                        case 6: //vedi_estrazione
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            
                            /* CONTROLLO SESSION ID*/
                            if(num_pieces == 3)
                                ret = check_session_id(command_piece[2], current_user_des.session_id);
                            else if(num_pieces == 4)
                                ret = check_session_id(command_piece[3], current_user_des.session_id);
                            else {
                                printf("[ERR] troppi argomenti per la vedi_estrazione");
                                strcpy(response, "ERROR wrong_number_parameter");
                                break;
                            }
                            

                            //Gestione errore session id e gestione risposta per tipologia di estrazione richiesta dal client
                            if(ret == -1)
                                strcpy(response, "ERROR wrong_session_id");
                            else if(num_pieces == 3) 
                                handle_vedi_estrazione_tot(command_piece[1], file_extractions_pointer, response);
                            else if(num_pieces == 4)
                                handle_vedi_estrazione_ruota(command_piece[1], command_piece[2], file_extractions_pointer, response);
                             
                            break;
                        case 7: //vedi_vincite
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            
                            //Controllo session_id
                            ret = check_session_id(command_piece[1], current_user_des.session_id);
                            if(ret == -1){
                                 strcpy(response, "ERROR wrong_session_id");
                            }
                            
                            //riciclo funzione vedi_giocata (la gestione diversa avverrà lato client)
                            handle_vedi_giocata(response, 0 , current_user_des.namefile);
                            break;
                        case 8: //esci
                            printf("[INFO] Client ha richiesto operazione %s\n", commands_vect[command_id - 1]);
                            
                            //Controllo session_id e gestione uscita
                            ret = check_session_id(command_piece[1], current_user_des.session_id);
                            if(ret == -1) {
                                strcpy(response, "ERROR wrong_session_id");
                            } else {
                                handle_exit(current_user_des.session_id, current_user_des.namefile, current_user_des.status_login); //devo aggiornare tutti i dati per memorizzare il logout
                                strcpy(response, "!esci");  
                            }
                            break;
                        default: //Gestione comandi sconosciuta comporta la disconnessione del client (gli eventuali errori sono già controllati lato client, se arriva un comando sconosciuto è da interpretare come segnale di errore)
                            printf("[ERR] Comando richiesto dal client inesistente\n");
                            
                            handle_exit(current_user_des.session_id, current_user_des.namefile, current_user_des.status_login);
                            printf("[INFO] Logout avvenuto con successo\n");
                            printf("[INFO] Chiusura connessione TCP per errore dovuto al client\n");
                            exit(-1);
                            break;
                    }


                    //Risposta al comando
                    len = strlen(response) + 1;
                    len_server_format = htons(len);


                    if(len > 20000){
                        printf("[ERR] Dimensione risposta massima superata.");
                        strcpy(response, "ERR buffer_overflow");
                        len = strlen(response) + 1;
                        len_server_format = htons(len);
                    }

                    //printf("[DEBUG] response: %s, len: %d\n", response, len);
                    
                    //Invio dimensione risposta
                    ret = send(client_socket_des, (void*)&len_server_format, sizeof(uint16_t), 0);
                    if(ret == -1) {
                        perror("[ERR] Send dimensione non riuscita: ");
                        exit(-1);
                    }

                    //Invio risposta
                    ret = send(client_socket_des, (void*)response, len, 0);
                    if(ret == -1) {
                        perror("[ERR] Send risposta non riuscita: ");
                        exit(-1);
                    }

                    if(command_id == 8) { 
                        printf("[INFO] Chiusura connessione TCP con client\n");
                        break;
                    }
                    
                    if(strcmp(response, "!esci 0") == 0){
                        printf("[INFO] Chiusura connessione TCP con client per eccesso di tentativi errati di accesso\n");
                        break;
                    }
                }

                close(client_socket_des);                
                exit(1);

            } else if(pid == -1) {
                //Errore nella fork
                perror("[ERR] Fork non riuscita: ");
                exit(-1);
            } else {
                //Processo padre che ascolta
                close(client_socket_des);
            }            
        }
    }
    
    return (EXIT_SUCCESS);
}
