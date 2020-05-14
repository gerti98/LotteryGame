/* 
 * File:   lotto_client.c
 * Author: Olgerti Xhanej
 *
 * Created on 13 dicembre 2019, 22:25
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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MSG_HELP_TOT "\n*********************** GIOCO DEL LOTTO **************************\nSono disponibili i seguenti comandi:\n\n1) !help <comando> --> mostra i dettagli di un comando\n2) !signup <username> <password> --> crea un nuovo utente\n3) !login <username> <password> --> autentica un utente\n4) !invia_giocata g --> invia una giocata g al server\n5) !vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0, 1} e permette di visualizzare le giocate passate '0' oppure le giocate attive '1' (ancora non estratte)\n6) !vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni sulla ruota specificata\n7) !vedi_vincite --> mostra le ultime vincite dell'utente\n8) !esci --> termina il client\n"
#define MSG_HELP_HELP "\nFormato: !help [<comando>]\nIndica tutti i comandi disponibili se non si specifica il secondo argomento. \nAltrimenti restituisce una descrizione più dettagliata del comando indicato nel secondo argomento\n"
#define MSG_HELP_SIGNUP "\nFormato: !signup <username> <password>\nPermette la registrazione dell' utente indicato dal parametro 'username' al gioco del Lotto.\n"
#define MSG_HELP_LOGIN "\nFormato: !login <username> <password>\nPermette ad un utente registrato di effettuare il login al servizio del gioco del Lotto.\nUna volta effettuato il login e' possibile iniziare ad inviare o vedere le proprie giocate, vedere le estrazioni, vedere le proprie vincite oppure semplicemente uscire dal servizio mediante il logout (comando !esci)\n"
#define MSG_HELP_INVIA_GIOCATA "\nFormato: !invia_giocata -r <ruote> -n <numeri_giocati> -n <scommesse_per_giocata>\nPermette ad un utente loggato di inviare una giocata al server del gioco del Lotto\nIl parametro 'ruote' può contenere i nominativi delle ruote (in minuscolo) oppure la dicitura 'tutte' per indicare tutte le ruote\nIl parametro 'numeri_giocati' può contenere da 1 a 10 numeri interi compresi tra 1 e 90, diversi tra di loro\nIl parametro 'scommesse_per_giocate' può contenere da 1 a 5 parametri che indicano l'importo giocato (eventualmente un importo non intero) per ogni tipologia di giocata (in ordine ESTRATTO, AMBO, TERNO, QUATERNA, CINQUINA)\nE' necessario aver effettuato il login prima di utilizzare questo comando\n"
#define MSG_HELP_VEDI_GIOCATE "\nFormato: !vedi_giocate <tipo>\nPermette di visualizzare le giocate precedentemente inviate dal giocatore\nIl parametro tipo può essere pari a '0' per indicare le giocate relative ad estrazioni già effettuate, \n'1' per indicare giocate attive (in attesa della prossima estrazione)\nE' necessario aver effettuato il login prima di utilizzare questo comando\n"
#define MSG_HELP_VEDI_ESTRAZIONE "\nFormato: !vedi_estrazione <n> [<ruota>]\nPermette di visualizzare le estrazioni effettuate dal server\nIl parametro 'n' indica il numero delle ultime estrazioni da visualizzare\nIl parametro opzionale <ruota> indica l'eventuale ruota dell'estrazione da visualizzare, altrimenti verranno mostrate le estrazioni di tutte le ruote\nE' necessario aver effettuato il login prima di utilizzare questo comando\n"
#define MSG_HELP_VEDI_VINCITE "\nFormato: !vedi_vincite\nMostra le vincite totali per tipo di giocata dell'utente richiedente\nE' necessario aver effettuato il login prima di utilizzare questo comando\n"
#define MSG_HELP_ESCI "\nFormato: !esci\nPermette di effettuare il logout dal servizio del gioco del lotto.\nE' necessario aver effettuato il login prima di utilizzare questo comando\n"
#define NUM_COMMANDS 8 //Numero di comandi accettabili dall'applicazione
#define MAX_PIECES 20000 //Numero massimo argomenti passabili ad un comando 
#define NUM_CITIES 11 //Numero di ruote
#define NUM_GIOCATE 5 
#define MAX_BUF_SIZE 20000


//Definizione buffer e stringhe globali
char* commands_vect[NUM_COMMANDS] = {"!help", "!signup", "!login", "!invia_giocata", "!vedi_giocate", "!vedi_estrazione", "!vedi_vincite", "!esci"};
char* city_vect[NUM_CITIES] = {"Bari", "Cagliari", "Firenze", "Genova", "Milano", "Napoli", "Palermo", "Roma", "Torino", "Venezia", "Nazionale"};
char* city_vect_minuscolo[NUM_CITIES] = {"bari", "cagliari", "firenze", "genova", "milano", "napoli", "palermo", "roma", "torino", "venezia", "nazionale"};
char* giocata_vect[NUM_GIOCATE] = {"estratto", "ambo", "terno", "quaterna", "cinquina"};
char* giocata_vect_maiusc[NUM_GIOCATE] = {"Estratto", "Ambo", "Terno", "Quaterna", "Cinquina"};

/*
 * Controlla che indirizzo inserito sia nel formato corretto n.n.n.n, con n intero compreso tra [0.255]
 * ritorna 0 se l'ip è corretto
 * ritorna -1 se l'ip non è corretto
 */
int check_ip_addr(char* ip_address) {
    char* work;
    int index = 0, int_work;
    
    //Controllo eventuali indirizzi ip riservati
    if(strcmp(ip_address, "255.255.255.255") == 0 || strcmp(ip_address, "0.0.0.0") == 0){
        printf("[ERR] Ip address inserito è riservato: non inserire indirizzi 255.255.255.255 o 0.0.0.0 \n");
        return -1;
    }
    
    //Analisi 'pezzo per pezzo'
    work = strtok(ip_address, ".");
    while(work != NULL){
        int_work = atoi(work);
        

        //printf("[DEBUG] n(%d): %d\n", index+1, int_work); 
        
        //Controllo che l'intero passato sia nel range considerato
        if(int_work < 0 || int_work > 255)
            return -1;
        
        index++;
        work = strtok(NULL, ".");
    }

    //Controlla che l'ip sia costituito da 4 interi in notazione puntata
    if(index != 4) 
        return -1;
    else
        return 0;   
}

/* 
 * Interpreta la stringa inviata come un comando, controllandone il formato
 * ritorna:
 * -1 -> Comando non trovato
 * i >= 0 -> Comando i-esimo trovato
 */
int find_command(char* command) {
    
    int index = 0;
    
    //Ricerca all'interno del vettore commands_vect il comando passato
    while(index < NUM_COMMANDS){
        if(strcmp(command, commands_vect[index]) == 0)
            return index + 1;
        index++;
    }
    
    return -1;    
}

/*
 * Funzione che ritorna la posizione della stringa str all'interno di vettore di stringhe vector di lunghezza massima size
 * ritorna -1 in caso di fallimento nella ricerca 
 */
int get_index_by_name(char* str, char** vector, int size) {
    int i=0;
    for(i=0; i<size; i++){
        if(strcmp(str, vector[i]) == 0)
            return i;
    }
    
    //Non è stata trovata alcuna uguaglianza
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
 * Funzione che controlla formato comando !invia_giocata
 * ritorna
 * -1: se il formato non è corretto (vedi comando !help !invia_giocata per dettagli sul formato)
 * 0: altrimenti
 */
int check_invia_giocata(char** command_piece, int num_pieces) { 
    int pos_r = 1, pos_n = 0, pos_i = 0; //Variabili per indicare posizione argomenti -r, -n, -i in modo da dare una valenza semantica al controllo
    int index = 0; //Variabile per scansionare i vari argomenti
    int id; //Serve per controllo esistenza nomi ruote 
    int num; //Serve per scansionare i numeri giocati (es. controllo interezza)
    double importo; //Serve per scansionare gli importi giocati
    int numeri_giocati[10]; //Lista numeri giocati (per controllo eventuali doppioni)
    int ruote_giocate[NUM_CITIES]; //ruote_giocate[i] = 1 -> ruota i-esima giocata (per controllo eventuali doppioni)
    int ret; //Ritorno funzione sscanf
    int n; //Ritorno lunghezza buffer
    int flag_giocata_nulla = 0; //Flag settato a 1 se la giocata da inviare non presenta tutti importi a 0
    
    memset(numeri_giocati, 0, sizeof(int)*10);
    memset(ruote_giocate, 0, sizeof(int)*NUM_CITIES);
    
    //Controlli preliminari
    if(num_pieces < 7) {
        printf("[ERR] Numero parametri invia_giocata insufficiente\n");
        return -1;
    }
    
    //Sicuramente il secondo parametro deve essere -r, se così non fosse restituisco errore
    if(strcmp(command_piece[1], "-r") != 0) {
        printf("[ERR] Posizione -r sbagliata\n");
        return -1;
    }

    //Trovo posizione -n -i
    for(index = 2; index < num_pieces && (pos_n == 0 || pos_i == 0); index++) {
        if(strcmp(command_piece[index], "-n") == 0) {
            if(pos_n == 0)
                pos_n = index;
            else{ //Controllo di non aver inserito più caratteri -n
                printf("[ERR] Inseriti troppi parametri -n\n");
                return -1;
            }                 
        }
        
        if(strcmp(command_piece[index], "-i") == 0) {
            if(pos_i == 0)
                pos_i = index;
            else { //Controllo di non aver inserito più caratteri -i
                printf("[ERR] Inseriti troppi parametri -i\n");
                return -1;
            }                 
        }
    }
    
    //printf("[DEBUG] pos_r: %d, pos_n: %d, pos_i: %d\n", pos_r, pos_n, pos_i);
    
    //Se -i precede -n è un errore secondo il formato del comando
    if(pos_i < pos_n) {
        printf("[ERR] Errore ordinamento -i -n\n");
        return -1;
    }
    

    //Non sono stati trovati parametri -n o -i
    if(pos_n == 0 || pos_i == 0) {
        return -1;
    }
    
    //Controllo esistenza di parametri 'vuoti' es. !invia_giocata -r -n 1 2 3 4 5 -i 1 2 4
    if((pos_n - pos_r) == 1 || (pos_i - pos_n) == 1 || (pos_i == num_pieces - 1)) {
        printf("[ERR] Qualche parametro vuoto intercettato\n");
        return -1;
    }
        
    //Controllo argomenti che seguono -r
    if((pos_n - pos_r) == 2 && strcmp(command_piece[pos_r + 1], "tutte") == 0) { 
        //Il parametro 'tutte' può essere inserito solamente da solo, altrimenti si controlla se ogni campo dopo -r è valido

        //printf("[DEBUG] parametro tutte trovato\n");
    } else {
        for(index = pos_r + 1; index < pos_n; index++) {  
            //Controllo che ogni parametro sia una ruota valida (vettore city_vect_minuscolo)
            id = get_index_by_name(command_piece[index], city_vect_minuscolo, NUM_CITIES);

            //Caso ruota non trovata
            if(id == -1) {
                printf("[ERR] Parametro numero %d: ruota non trovata (le ruote vanno scritte in minuscolo)\n", index);
                return -1;
            }

            //Caso doppione di ruota valida
            if(ruote_giocate[id] == 1){
                printf("[ERR] Trovato doppione di ruota: %s\n", command_piece[index]);
                return -1;
            }

            //Aggiorno flag ruote giocate
            ruote_giocate[id] = 1;
        }
    }
    
    //Controllo numeri giocati
    if(pos_i - pos_n >= 12) {
        printf("[ERR] Si possono inserire al massimo dieci numeri diversi\n");
        return -1;
    }
        
        
    //Controllo parametri -n
    for(index = pos_n + 1; index < pos_i; index++) {

        num = atoi(command_piece[index]);
        ret = sscanf(command_piece[index], "%d%n", &num, &n);
        
        //Controllo interezza numero inserito
        if(ret != 1 || n != strlen(command_piece[index])){
            printf("[ERR] IL seguente numero inserito non e' intero: %s\n", command_piece[index]);
            return -1;
        }
        
        //Controllo che numero inserito sia nel range [1, 90]
        if(num < 1 || num > 90) {
            printf("[ERR] Parametro %d: numero %d invalido\n", index, num);
            return -1;
        }
        
        //Controlla se ho indicato numeri doppioni
        if(is_present(num, numeri_giocati, index - pos_n) == 0){
            printf("[ERR] Doppione trovato: %d\n", num);
            return -1;
        }
        
        numeri_giocati[index - pos_n - 1] = num;
    }
    
    //Controllo parametri -i
    for(index = pos_i + 1; index < num_pieces; index++) {  
        importo = atof(command_piece[index]);

        //Controllo che importo giocato sia positivo
        if(importo < 0 ) {
            printf("[ERR] Parametro %d: importo giocata invalido (inserire importo positivo)\n", index);
            return -1;
        }     
        
        //Se ho inserito un importo > 0 allora la giocata a questo punto si può considerare significativa
        if(importo != 0){
            flag_giocata_nulla = 1;
        }
    }


    //Controllo che numeri giocati siano compatibili con la giocata scommessa (es. non posso giocare una cinquina con 4 numeri indicati)
    if((num_pieces - pos_i) > (pos_i - pos_n)) {
        printf("[ERR] Parametri -i sono maggiori dei numeri giocati -n\n");
        return -1;
    }
    
    //Non si può scommettere oltre la cinquina
    if((num_pieces - pos_i) > 6){
        printf("[ERR] Inseriti troppi parametri -i (max 5)\n");
        return -1;
    }
    
    //Controllo che abbia giocato effettivamente qualcosa
    if(flag_giocata_nulla == 0){
        printf("[ERR] Inserita giocata con tutti importi nulli\n");
        return -1;
    }
    
    return 0;
    
}


/*
 * Interpreta il vettore di stringhe command_piece in relazione al command_id passato. 
 * Controlla che il numero/formato degli argomenti sia compatibile con il comando interpretato 
 * ritorna:
 * -1 -> in caso di formato non compatibile
 * -2 -> in caso di operazione vietata per utente che ha effettuato il login (la !login e la !signup non possono essere eseguite una volta loggato)
 * -3 -> in caso di operazione vietata per utente che non ha ancora effettuato il login (tutte le altre operazioni, eccetto !login, !signup e !help)
 * 0 -> in caso di successo
 */
int check_format_command(char** command_piece, int command_id, int num_pieces, int already_logged) {    
    //printf("[DEBUG] Inizio check: num_pieces: %d\n", num_pieces);
    int n, ret, ret_scanf, num;
    
    switch(command_id){
        case 1: //help
            if(num_pieces > 2 || num_pieces < 1) //Controllo numeri parametri
                return -1;
            else if(num_pieces == 2 && find_command(command_piece[1]) == -1) //Controllo che eventuale secondo parametro sia un comando
                return -1;
            break;
        case 2: //signup
        case 3: //login 
            if(already_logged == 1) //Controllo che utente non abbia ancora effettuato login
                return -2;
            
            if(num_pieces != 3) //Controllo numero parametri
                return -1;
            //Controllo username vietati (già utilizzati per altre funzioni)
            else if(strcmp(command_piece[1], "extractions") == 0 || strcmp(command_piece[1], "namefile_list") == 0 || strcmp(command_piece[1], "banned_users") == 0){ //Rimozione username dedicati
                printf("\n[ERR] Indicato username dedicato, inserire un altro username");
                return -1;
            }
            break;
        case 4: //invia_giocata
            if(already_logged == 0) //Controllo che utente abbia già effettuato il login
                return -3;
            
            //Comando molto complesso, si rimanda controllo a funzione secondaria
            ret = check_invia_giocata(command_piece, num_pieces);
            return ret;

        case 5: //vedi_giocate
            if(already_logged == 0) //Controllo che utente abbia già effettuato il login
                return -3;
            
            if(num_pieces != 2) //Controllo numero parametri
                return -1;
            
            //Controllo che il parametro passato sia intero
            ret_scanf = sscanf(command_piece[1], "%d%n", &num, &n);
            if(ret_scanf != 1 || n != strlen(command_piece[1])){
                printf("\n[ERR] Inserito parametro non intero");
                return -1;
            }
                    
            //Controllo che il parametro numerico sia 0 o 1
            if(num != 1 && num != 0)
                return -1;
            break;
        case 6: //vedi_estrazione
            if(already_logged == 0) //Controllo che utente abbia già effettuato il login
                return -3;
            
            if(num_pieces != 2 && num_pieces != 3) //Controllo numero parametri
                return -1;
            
            //Controllo interezza parametro numerico
            ret_scanf = sscanf(command_piece[1], "%d%n", &num, &n);
            if(ret_scanf != 1 || n != strlen(command_piece[1])){
                printf("\n[ERR] Inserito parametro non intero");
                return -1;
            }
            
            //Controllo che parametro numerico sia significativo per comando (deve essere >= 1)
            if(num < 1)
                return -1;
            
            break;
            
        case 7: //vedi_vincite
        case 8: //esci
            if(already_logged == 0) //Controllo che utente abbia già effettuato il login
                return -3;
            
            if(num_pieces != 1) //Controllo numero parametri
                return -1;

    }
    return 0;
}



/*
 * Funzione per gestione comando !help, in base al secondo parametro (se presente), stampa un messaggio diverso
 */
void execute_help(int command_id) {
    //Gestione stampa informazioni aggiuntive in base al comando digitato
    switch(command_id){
        case 1:
            printf(MSG_HELP_HELP);
            break;
        case 2:
            printf(MSG_HELP_SIGNUP);
            break;
        case 3:
            printf(MSG_HELP_LOGIN);
            break;
        case 4:
            printf(MSG_HELP_INVIA_GIOCATA);
            break;
        case 5:
            printf(MSG_HELP_VEDI_GIOCATE);
            break;
        case 6:
            printf(MSG_HELP_VEDI_ESTRAZIONE);
            break;
        case 7:
            printf(MSG_HELP_VEDI_VINCITE);
            break;
        case 8:
            printf(MSG_HELP_ESCI);
            break;
        default:
            printf("\n\n[ERR] Parametro indicato dal comando !help non riconosciuto\n\n");
            break;
    }
}

/*
 * Funzione adibita a stampare secondo il pattern delle specifiche le estrazioni richieste
 */
void handle_response_vedi_estrazione(char* response, int type) {
    char c;
    int i=3; //Indire scorrimento risposta
    int num_lines = 0; //Contatore righe (per esigenze di stampa)    
    int flag_new_line;    
    int num_lines_estrazione; //Numero di linee per estrazione (se ho indicato un ruota specifica sarà un numero minore)
    
    printf("\n");

    //Setto correttamente il parametro se è stata specificata o meno la ruota
    if(type == 0)
        num_lines_estrazione = 12;
    else
        num_lines_estrazione = 2;
    

    //Analizzo carattere per carattere l'estrazione
    do{
        c = response[i];
        i++;
        if(c == '\n')
            num_lines++;
        
        //Se sono alla prima riga dell'estrazione per esigenza di stampa aggiungo un \n
        if((num_lines % num_lines_estrazione) == 0 && flag_new_line == 0){
            putchar('\n');
            flag_new_line = 1;
        }
        
        //Se sono ad una riga diversa dalla prima trasformo gli spazi in \t per esigenze di stampa
        if(c == ' ' && (num_lines % num_lines_estrazione) != 0){
            putchar('\t');
            flag_new_line = 0;
        }
        else
            putchar(c);
        
            
    }while(c != '\0');
}

/*
 * Funzione che svolge il compito di deserializzare il messaggio di risposta 
 * del comando vedi_giocata ed il compito di stampare il risultato in un formato facilmente leggibile (come da specifiche)
 */
void handle_response_vedi_giocate(char** response_piece, int num_pieces) {
    int index = 1; //Indice di scorrimento per il vettore
    int flag = -1; //Variabile per tenere conto dello stato corrente, relativo alle letture dei tag -r, -n, -i per gestire correttamente i flussi di informazioni
    int id; //Id per riconoscere la ruota all'interno del vettore apposito (city_vect)
    int line = 1; //Contatore che tiene conto del numero di giocata ai fini della stampa
    int i_index; //Indice che tiene conto della posizione di -i all'interno del comando di risposta


    //printf("[DEBUG] Inizio handle_response_vedi_giocate\n");
    while(index < num_pieces - 1) {
        
        //Se ho analizzato la vincita di tutta la riga corrente passo alla prossima riga se esiste
        if(strcmp(response_piece[index], "-v") == 0 || response_piece[index][0] == '?') {
            
            while(strcmp(response_piece[index], "-r") != 0 && index < num_pieces - 1){ 
                index++;
                //printf("[DEBUG] response_piece[%d]: %s\n", index, response_piece[index]);
               
            }

            if(index >= num_pieces - 1)
                break;
        }
         
        //printf("[DEBUG] response_piece[%d]: %s\n", index, response_piece[index]);
        
        //Cerco il campo -r ed adatto la stampa per la ruota    
        if(strcmp(response_piece[index], "-r") == 0 || strcmp(response_piece[index], "\n-r") == 0) {
            //printf("[DEBUG] -r trovato\n");
            flag = 0;
            printf("\n%d) ", line++);

        //Cerco il campo -n ed adatto la stampa per i numeri giocati
        } else if(strcmp(response_piece[index], "-n") == 0) {
            //printf("[DEBUG] -n trovato\n");
            flag = 1;

        //Cerco il campo -i ed adatto la stampa per l'importo scommesso
        } else if(strcmp(response_piece[index], "-i") == 0) {
            //printf("[DEBUG] -i trovato\n");
            flag = 2;
            i_index = index;
        } else {

            //Gestione stampa ruota
            if(flag == 0) {
                id = get_index_by_name(response_piece[index], city_vect_minuscolo, NUM_CITIES);
                if(id != -1)
                    printf("%s ", city_vect[id]);
                else if(id == -1 && strcmp(response_piece[index], "r") != 0) //Hotfix problema di stampa
                    printf("Tutte ");

            //Gestione stampa numero
            } else if(flag == 1) {
                printf("%s ", response_piece[index]);
            
            //Gestione stampa importo
            } else if(flag == 2) {
                if(strcmp(response_piece[index], "\n") == 0){
                    printf("\n");
                    break;
                }
                
                //Stampo l'importo solo se non è nullo
                if(strcmp(response_piece[index], "0") != 0)
                    printf("* %0.2f %s ", atof(response_piece[index]), giocata_vect[index - i_index - 1]);
            }
        }
            
        index++;
    }

    printf("\n"); 
}

/*
 * Funzione che ha il compito di deserializzare messaggio relativo alle giocate ed alle vincite, stampando il tutto secondo il formato delle specifiche
 */
void handle_response_vedi_vincite(char** response_piece, int num_pieces){
    int i; //Indice scorrimento vettore response_piece
    float somma_vincite[5]; //Vettore che conterrà le vincite totali per tipologia di giocata
    int index_inizio_riga = 0; //Indice i-esimo relativo all'inizio della 'riga' corrente (ogni riga rappresenta una giocata ed una vincita)
    int flag_inizio_riga = 0; //Flag relativo a salvataggio index inizio riga (se 1 ho giò salvato index di inizio riga
    int flag_timestamp = 0; //Flag relativo a ricerca campo timestamp all'interno della riga
    time_t timestamp;
    int flag_equal_timestamp = 0; //Se è settato indica che l'ultimo timestamp trovato è uguale a quello della riga precedente (per esigenza di formattazione questa informazione va ricavata)
    int flag_r = 0; //Flag trovato -r nella riga corrente
    int flag_n = 0; //Flag trovato -n nella riga corrente
    int flag_v = 0; //Flag trovato -v nella riga corrente
    int flag_i = 0; //Flag trovato -i nella riga corrente
    int flag_t = 0; //Flag trovato -t nella riga corrente
    int index_v = 0, index_i = 0; //Indice campo -v e campo -i corrente
    int id;
    int flag_inizio_stampa = 0; //Flag per esigenze di stampa
    int n, num;
    //int ret_scanf;
    int giocata_effettuata[5];
    
    struct tm tm; //Struttura per stampare correttamente il timestamp

    memset(somma_vincite, 0, sizeof(float)*5);
    memset(giocata_effettuata, 0, sizeof(int)*5);
    
    for(i = 1; i < num_pieces; i++){
        //printf("[DEBUG] response_piece[%d]: %s\n", i, response_piece[i]);
        //Per ogni giocata
        
        //Salvo posizione iniziale "riga"
        if(flag_inizio_riga == 0){
            index_inizio_riga = i;
            flag_inizio_riga = 1;
            //printf("[DEBUG] Inizio riga: %d", index_inizio_riga);
        }
        
        //Ottengo timestamp e controllo se era diverso da quello precedente
        if(flag_timestamp == 0){            
            if(strcmp(response_piece[i], "-t") == 0){              
                if(timestamp != atoi(response_piece[i+1]))
                    timestamp = atoi(response_piece[i+1]);
                else
                    flag_equal_timestamp = 1; 
                
                flag_timestamp = 1;
                i = index_inizio_riga-1; //Torno ad inizio riga per stampare la giocata (il timestamp infatti stava in fondo)
                continue;
            } else {
                continue;
            }
        }
        
        //Se timestamp è diverso diverso adatto la stampa al nuovo timestamp
        if(flag_equal_timestamp == 0){
            tm = *localtime(&timestamp);

            //Primo timestamp è sicuramente diverso dal precedente, non devo stampare gli asterischi
            if(flag_inizio_stampa == 1)
                printf("************************************\n");
                        
            //Stampa formato estrazione in base al formato dei minuti (se < 10 devo aggiungere uno 0)
            if(tm.tm_min >= 10)    
                printf("Estrazione del %d-%d-%d ore %d:%d \n", tm.tm_mday,  tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);
            else
                printf("Estrazione del %d-%d-%d ore %d:0%d \n", tm.tm_mday,  tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);

            flag_equal_timestamp = 1;
            flag_inizio_stampa = 1;
        }
        
        //Se arrivo a fine 'riga' resetto tutti i flag        
        if(response_piece[i][0] == '?') {
            //printf("[DEBUG] Trovata fine riga");
            
            index_inizio_riga = 0;
            flag_timestamp = 0;
            flag_inizio_riga = 0;
            flag_equal_timestamp = 0;
            flag_r = flag_n = flag_v = flag_i = flag_t = 0;
            memset(giocata_effettuata, 0, sizeof(int)*5);
            printf("\n");
            continue;
        }
        
        
        
        //Set dei vari flag relativi ai parametri -r, -t, -n, -v, -i per adattare la stampa alla semantica del parametro
        if(strcmp(response_piece[i], "-r") == 0 || strcmp(response_piece[i], "\n-r") == 0) {
            //printf("[DEBUG] Trovato -r\n");
            flag_r = 1;
            continue;
        } 
        else if(strcmp(response_piece[i], "-t") == 0) {
            flag_t = 1;
            continue;
        }
        else if(strcmp(response_piece[i], "-n") == 0) {
            flag_n = 1;
            continue;
        } 
        
        else if(strcmp(response_piece[i], "-v") == 0) {
            
            //Salvo indice parametro -v (ordine parametri ha valenza semantica)
            index_v = i;
             
            flag_v = 1;
            printf("  >>   "); 
            continue; 
        } 
        else if(strcmp(response_piece[i], "-i") == 0){
            flag_i = 1;
            index_i = i; //Salvo indice parametro -i (ordine parametri ha valenza semantica)
            continue;
        }
        
        //Setto flag importi giocati per tipo di giocata 
        if(flag_i == 1 && flag_v != 1){
            if(atof(response_piece[i]) != 0)
                giocata_effettuata[i - index_i - 1] = 1;
        }
        
        
        //Stampa delle vincite per ogni tipo di giocata
        if(flag_v == 1 && flag_t != 1 && giocata_effettuata[i - index_v - 1] == 1){
            //printf("[DEBUG] i: %d, index_v: %d\n", i, index_v);
            
            //Controllo interezza vincita (per esigenze di stampa)
            sscanf(response_piece[i], "%d%n", &num, &n);
            if(num - atof(response_piece[i]) == 0)
                printf("%s %d   ", giocata_vect_maiusc[i - index_v - 1], num);
            else
                printf("%s %0.2f   ", giocata_vect_maiusc[i - index_v - 1], atof(response_piece[i]));
            
            //Aggiorno somma totale delle vincite per giocata
            somma_vincite[i-index_v-1] += atof(response_piece[i]); 
            continue;
        }
          
        
        //Stampa dei numeri giocati
        if(flag_n == 1){
            if(flag_i == 0)
                printf("%d ", atoi(response_piece[i]));
            continue;
        }

        //Stampa delle ruote giocate
        if(flag_r == 1){

            id = get_index_by_name(response_piece[i], city_vect_minuscolo, NUM_CITIES);
            if(id == -1 && strcmp(response_piece[i], "r") != 0) //Se ruota non esiste è sicuramente il parametro Tutte //Hotfix problema di stampa aggiunto
                printf("Tutte ");
            else if( id != -1)
                printf("%s ", city_vect[id]);
        }
    }
    
    //Stampa resoconto totale delle vincite dell'utente
    printf("\n\nVincite su ESTRATTO: %0.2f\nVincite su AMBO: %0.2f\nVincite su TERNO: %0.2f\nVincite su QUATERNA: %0.2f\nVincite su CINQUINA: %0.2f\n", somma_vincite[0], somma_vincite[1], somma_vincite[2], somma_vincite[3], somma_vincite[4]);
}

/*
 * 
 */
int main(int argc, char** argv) {
    
    int socket_des, ret, len;
    uint16_t len_server_format; //Variabile destinata a contentere lunghezza msg in formato server
    struct sockaddr_in server_addr; //Indirizzo del server
    char server_ip_address[1024]; //Ip address del server (passato per argomento)
    char response[MAX_BUF_SIZE]; //Buffer per ricezione risposta dal server
    char response_work[MAX_BUF_SIZE]; //Buffer per lavoro risposta dal server
    uint32_t port_number;
    char session_id[11];
    int attempt_remained = 3; //Tentativi rimasti di accesso prima di essere bannato per 30 minuti
    int already_logged = 0; //Identifica se il client ha effettuato il login (1) o meno (0)s
    int type_vedi_estrazione; //Variabile per definire comportamento stampa operazione vedi_estrazione
    int ret_scanf, n; //Variabili utili a determinare se un numero è intero o meno
    int i;

    //Gestione array di stringhe con prima inizializzazione   
    char** command_piece = (char**)malloc(sizeof(char*) * MAX_PIECES);
    for(i = 0 ; i < MAX_PIECES ; i++)
         command_piece[i] = (char*)malloc(sizeof(char) * 100);

    
    char** response_piece = (char**)malloc(sizeof(char*) * MAX_PIECES);
    for(i = 0 ; i < MAX_PIECES ; i++)
         response_piece[i] = (char*)malloc(sizeof(char) * 100);
 


    //Pulizia dei buffer di lavoro
    memset(response, 0, sizeof(char)*MAX_BUF_SIZE);
    memset(response_work, 0, sizeof(char)*MAX_BUF_SIZE);
    memset(server_ip_address, 0, sizeof(char)*1024);
    memset(session_id, 0, sizeof(char)*11);
    
    //Controllo parametri passati all'avvio del client
    if(argc != 3) {
       printf("[ERR] Numero argomenti passati invalido.\n[ERR] Il formato supportato è: ./lotto_client <IP server> <porta server> \n");
       exit(-1);
    } else {

        //Controllo interezza numero di porta
        ret_scanf = sscanf(argv[2], "%d%n", &port_number, &n);
        if(ret_scanf != 1 || n != strlen(argv[2])){
            printf("[ERR] Valore numero di porta inserito non e' un numero intero\n");
            exit(-1);        
        }
        
        //Controllo range numero di porta
        if(port_number < 1024 || port_number > 65535) {
            printf("[ERR] Numero porta indicato non permesso, inserire numero di porta nel range [1024, 65535]\n");
            exit(-1);
        }
        
        strcpy(server_ip_address, argv[1]);
        //printf("[DEBUG] port: %d, ip: %s\n", (int)port_number, server_ip_address); //DEBUG
        
        //Check formato ip address del server
        if(check_ip_addr(server_ip_address) == -1) {
            printf("[ERR] Formato ip_address non corretto: inserire ip_address nel formato n(1).n(2).n(3).n(4), dove n(i) è un intero nel range [0, 255] per ogni i da 1 a 4\n");
            exit(-1);
        }
        
    }
    
    //Creazione struttura dati socket_descriptor
    socket_des = socket(AF_INET, SOCK_STREAM, 0);    
    if(socket_des == -1) {
        perror("[ERR] Socket non riuscita: ");
        exit(-1);
    }
    
    //Configurazione dell'indirizzo del server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_port = htons(port_number);
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip_address, &server_addr.sin_addr);
        
    //Connessione al server
    ret = connect(socket_des, (struct sockaddr*)&server_addr, sizeof(server_addr));    
    if(ret == -1){
        perror("[ERR] Connessione al server non riuscita: ");
        exit(-1);
    }
    
    /* CHECK CLIENT BANNATO */
    
    //Ricezione dimensione risposta di benvenuto
    ret = recv(socket_des, (void*)&len_server_format, sizeof(uint16_t), 0);
    if(ret == -1) {
        perror("[ERR] Recv dimensione non riuscita: ");
        exit(-1);
    } else if(ret == 0){
        printf("[ERR] Chiusura connessione TCP con il server\n");
        
        //Chiudo connessione con client in stato di errore
        close(socket_des);
        exit(-1);       
    }


    len = ntohs(len_server_format);
    //printf("[DEBUG] len_rcv: %d\n", len);

    //Ricezione risposta al comando
    ret = recv(socket_des, (void*)response, len, 0);
    if(ret == -1) {
        perror("[ERR] Recv risposta non riuscita: ");
        exit(-1);

    //Gestione chiusura connessione da parte del server 
    } else if(ret == 0){
        printf("[ERR] Chiusura connessione TCP con il server\n");
        
        //Chiudo connessione con client in stato di errore
        close(socket_des);
        exit(-1);       
    }

    //printf("[DEBUG] comando ricevuto: %s\n", response);
    
    //Se client è ancora bannato
    if(strcmp(response, "NO") == 0){
        printf("[ERR] Impossibile stabilire connessione con server in quanto il client è stato bannato per 30 minuti per eccesso di tentativi errati di accesso\n");
        close(socket_des);
        exit(-1);
    }
    
    //A questo punto la connessione al server si è stabilita    
    printf("[INFO] Connessione con server stabilita \n");
    printf(MSG_HELP_TOT);
    
    while(1) {
        int command_id = 0; //Id del comando (rispetto al vettore dei comandi)
        int command_id_help = 0; //Id del comando indicato nel comando help
        int command_num_pieces = 0; //Numero di argomenti passati al comando (comando stesso incluso)
        int response_num_pieces = 0; //Numero di argomenti ricevuti in risposta dal server
        char buffer_work[1024];

        char entire_command[1024]; //Intero comando inserito
        
        memset(buffer_work, 0, 1024);
        memset(entire_command, 0, 1024);
        memset(response_work, 0, MAX_BUF_SIZE);
        memset(response, 0, MAX_BUF_SIZE);

        printf("\n> ");
        fgets(buffer_work, 1024, stdin); //Prelevo intera riga da linea di comando
        
        //Gestione eventuale inserimento vuoto
        if(strcmp(buffer_work, "\n") == 0){
            printf("\n[ERR] Nessun comando inserito\n");
            continue;
        }
        

        buffer_work[strlen(buffer_work) - 1] = '\0'; //Elimino il newline finale
        
        
        strncpy(entire_command, buffer_work, strlen(buffer_work)); //Ricopio il comando iniziale prima della break_command_in_pieces

        /** Processazione Comando **/        
        command_num_pieces = break_command_in_pieces(buffer_work, command_piece, MAX_PIECES);
       

        //Gestione numero parametri rispetta i vincoli
        if(command_num_pieces == 0) {
            printf("[ERR] Nessun comando inserito\n");
            continue;
        } else if(command_num_pieces == -1) {
            printf("[ERR] Numero di argomenti supera il massimo previsto(1000)\n");
            continue;
        }

        //printf("[DEBUG] command_piece[0]: %s, num_pieces: %d\n", command_piece[0], command_num_pieces);
        
        //Cerco se il comando inserito è uno di quelli disponibili
        command_id = find_command(command_piece[0]);        
        if(command_id == -1) {
            printf("\n[ERR] Comando specificato non trovato\n");
            continue;
        }
        
        //printf("[DEBUG] command_id: %d\n", command_id);
        
        //Controllo che formato comando specificato da command_id sia compatibile con le specifiche (!help <comando> per chiarimenti)
        ret = check_format_command(command_piece, command_id, command_num_pieces, already_logged);

        if(ret == -1) {
            printf("\n[ERR] Formato comando %s errato. Reinserire comando nel formato corretto\n", commands_vect[command_id-1]);
            continue;
        } else if(ret == -2){
            printf("\n[ERR] Utente si è già loggato, impossibile eseguire comando %s \n", commands_vect[command_id-1]);
            continue;
        } else if(ret == -3){
            printf("\n[ERR] Operazione vietata: login necessario per eseguire comando %s \n", commands_vect[command_id-1]);
            continue;
        }
        
        //Controllo se comando può essere eseguito in locale, senza interazione con il server (comando !help)
        if(command_id == 1) {
            //Se ho inserito solo un parametro voglio la lista di tutti i comandi disponibili
            if(command_num_pieces == 1)
                printf(MSG_HELP_TOT);
            else if(command_num_pieces == 2){
                command_id_help = find_command(command_piece[1]);
                execute_help(command_id_help);
            } else {
                printf("[ERR] Numero parametri del comando !help non riconosciuto\n");
            }
             
            continue;
        }
        
        //Nel caso in cui il comando sia un !vedi_estrazione, memorizzo se ho indicato o meno la ruota (per esigenze di stampa)
        if(command_id == 6){
            if(command_num_pieces == 2)
                type_vedi_estrazione = 0;
            else if(command_num_pieces == 3)
                type_vedi_estrazione = 1;
        }
        

        //Aggiunta del session_id ai comandi che si possono effettuare dopo il login
        if(command_id > 2 && command_id <= 8) {
            entire_command[strlen(entire_command)] = ' ';
            strcat(entire_command, session_id);
        }
        
        /** Invio al server il comando**/
        //printf("[DEBUG] comando inviato: %s, buffer iniziale: %s\n", buffer_in, entire_command);
        //printf("[DEBUG] risultato strlen: %d\n", strlen(entire_command));
       
        //Gestione lunghezza comando da mandare
        entire_command[strlen(entire_command)] = '\0';
        len = strlen(entire_command) + 1;
        len_server_format = htons(len);
        
        //printf("[DEBUG] len inviata: %d\n", len);
        //printf("[DEBUG] comando inviato: %s\n", entire_command);
        
        //Invio dimensione
        ret = send(socket_des, (void*)&len_server_format, sizeof(uint16_t), 0);
        if(ret == -1) {
            perror("[ERR] Send dimensione non riuscita: ");
            exit(-1);
        }
        
        //Invio comando
        ret = send(socket_des, (void*)entire_command, len, 0);
        if(ret == -1) {
            perror("[ERR] Send comando non riuscita: ");
            exit(-1);
        }
        
        
        //Ricezione dimensione risposta
        ret = recv(socket_des, (void*)&len_server_format, sizeof(uint16_t), 0);
        if(ret == -1) {
            perror("[ERR] Recv dimensione non riuscita: ");
            exit(-1);

        //Gestione chiusura connessione da parte del server 
        } else if(ret == 0){
            printf("[ERR] Chiusura connessione TCP con il server\n");
            
            //Chiudo connessione con client in stato di errore
            close(socket_des);
            exit(-1);       
        }
        
        len = ntohs(len_server_format);
        //printf("[DEBUG] len_rcv: %d\n", len);
        
        //Ricezione risposta al comando inviato
        ret = recv(socket_des, (void*)response, len, 0);
        if(ret == -1) {
            perror("[ERR] Recv risposta non riuscita: ");
            exit(-1);

        //Gestione chiusura connessione da parte del server   
        } else if(ret == 0){
            printf("[ERR] Chiusura connessione TCP con il server\n");
            
            //Chiudo connessione con client in stato di errore
            close(socket_des);
            exit(-1);       
        }
        
        //printf("[DEBUG] comando ricevuto: %s\n", response);
        strcpy(response_work, response);  //Ricopio la risposta prima della break_command_in_pieces
        
        //printf("[DEBUG] response_piece[0]: %c%c", response_work[0], response_work[1]);
        
        //Il comando va spezzato e ricopiato nel vettore di stringhe 
        response_num_pieces = break_command_in_pieces(response, response_piece, MAX_PIECES);
        if(response_num_pieces == -1) {
            printf("[ERR] Numero di argomenti supera il massimo previsto(1000)\n");
        }
        
        //printf("[DEBUG] resp[0]: %s, resp_num_pieces: %d\n", response_piece[0], response_num_pieces);
        

        //Primo parametro può essere OK, ERROR oppure !esci
        //Nel caso sia OK la risposta del server al comando ha avuto successo
        if(strcmp(response_piece[0], "OK") == 0) {

            //Si procede alla stampa dell'eventuale risposta al comando inviato
            switch(command_id){
                case 2: //Signup
                    printf("\n[INFO] Registrazione effettuata con successo\n");
                    break;
                case 3: //login
                    strcpy(session_id, response_piece[1]);
                    //printf("[DEBUG] session_id_rcv: %s, session_id: %s\n", response_piece[1], session_id);
                    printf("\n[INFO] Login effettuato con successo\n");
                    already_logged = 1; 
                    break;
                case 4:
                    printf("\n[INFO] Giocata inviata con successo\n");
                    break;
                case 5:
                    handle_response_vedi_giocate(response_piece, response_num_pieces);
                    break;
                case 6:
                    //printf("[DEBUG] response_num_pieces: %d\n", response_num_pieces);
                    handle_response_vedi_estrazione(response_work, type_vedi_estrazione);
                    break;
                case 7:
                    //printf("[DEBUG] Risposta vedi vincite\n");
                    handle_response_vedi_vincite(response_piece, response_num_pieces);
                    break;
                default:
                    printf("\n[ERR] Comando inviato non riconosciuto\n");
                    break;
            }
        //Caso in cui il server abbia mandato messaggio di chiusura della connessione
        } else if(strcmp(response_piece[0], "!esci") == 0) {
            if(strcmp(response_work, "!esci 0") == 0) //Caso in cui utente è stato bannato per superamento tentativi
                printf("[INFO] Sei stato bannato per 30 minuti a causa dei ripetuti tentativi di accesso errati\n");
            
            close(socket_des);
            printf("[INFO] Chiusura connessione TCP con server\n");
            exit(1);

        //Caso in cui sia arrivato un messaggio di errore    
        } else { 
            printf("\n[ERR] C'e' stato un errore. Comando: %s, errore: %s\n", commands_vect[command_id-1], response_piece[1]);
            
            //Nel caso si tratti di un errore al login se diminuiscono i tentativi permessi di errore (arrivati a 0 viene bannato l'ip del client per 30 minuti)
            if(strcmp(response_piece[1], "wrong_credentials") == 0){
                attempt_remained--;
                printf("[INFO] Tentativi rimasti: %d\n", attempt_remained);
            }
        }
     
    }
    
    return (EXIT_SUCCESS);
}
