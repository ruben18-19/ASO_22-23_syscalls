#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h> //printf
#include <unistd.h> //read, write, STDIN_FILENO, execvp
#include <string.h> // memcpy
#include <sys/wait.h>



// You need to use optstring = ":d:f:", with an initial :, to allow optional arguments. Something very annoying is that, while foo -d and foo -f arg work as expected, foo -d -f arg mistakes -f for the optarg to -d .... Any neat workwaround ? – 
// phs
// Jun 2, 2016 at 16:48
// 2
// @phs, I think you have to manually check if optarg looks like an option (e.g. optarg[0]=='-'), and if so then rewind optind to read it (optind-=1;). 

// usar strtok para separar líneas

#define MAX_LINE_SIZE 128
#define ERR_LINE_SIZE "Error: Tamaño de línea mayor que "
#define MAX_BYTES_PER_READ 16
#define MAX_LINES_IN_BUFFER 9   // establecemos a 9 (aunque lo veo poco probable => órdenes de 1 carácter + \n)
#define MAX_PROC 8
#define MIN_PROC 1

int     processArguments(int argc, char **argv);

/* MAIN AUX FUNCTIONS */
void    checkLineSize(int);
char * splitInputInCommands(char *input, int inputLength, char *commands[], int *startOfNextCommand, int *startOfUnprocessedChars, int *commandsNo);
void    processLineCommand(char *);

/* CHILD FUNCTIONS */
int     execute(char *command);
int     splitCommand(char *command, char *tokens[]);

/* MAIN */
int main(int argc, char *argv[]) {
    char *bufferAcumulativo = (char *)malloc((MAX_LINE_SIZE+1)*sizeof(char));      // MAX_LINE_SIZE + '\0'
    char buffer[16] = "";
    ssize_t bytesRead = 0;
    ssize_t totalBytesReadPerLine = 0;
    char **commands = (char **)malloc(MAX_LINES_IN_BUFFER*sizeof(char *));     //reservamos espacio para MAX_LINES_IN_BUFFER char * (strings)
    int startOfNextCommand = 0;
    int startOfUnprocessedChars = 0;
    int currentPosition = 0;
    int lineFound = 0;
    int round = 0;
    int absCommandNo = 1;
    int commandsNo;
    int nProc;
    int freeThreads;
    // int nProc = 2;
    // int nProcRunning = 0;

    // TODO - salir con fallo dentro de la función?
    if ((nProc = processArguments(argc,argv)) == -1) {
        exit(EXIT_FAILURE);
    }

    printf("freeThreads = %i\n",nProc);
    freeThreads = nProc;

    while ((bytesRead = read(STDIN_FILENO,buffer,MAX_BYTES_PER_READ))) {
        // ERROR READING
        if (bytesRead == -1) {
            perror("read()");
            exit(EXIT_FAILURE);
        }
        
        // printf("--------------------------------------");
        // printf("ROUND %d\n",round++);

        // si superamos el límite de 128 bytes/línea => ERR; se sale con EXIT_FAILURE dentro de la función
        checkLineSize(totalBytesReadPerLine += bytesRead);

        // copiamos en bufferAcumulativo los bytes leídos en esta iteración
        // puede que haya datos válidos en este buffer, esto está indicado por startOfNextCommand
        // printf("bufferAcumulativo=%s; buffer=%s\n",bufferAcumulativo,buffer);
        memcpy(bufferAcumulativo+startOfNextCommand,buffer,bytesRead);
        // printf("bufferAcumulativo='%s'; buffer='%s'\n",bufferAcumulativo,buffer);

        startOfNextCommand += bytesRead;
        bufferAcumulativo = splitInputInCommands(bufferAcumulativo, startOfNextCommand, commands, &startOfNextCommand, &startOfUnprocessedChars, &commandsNo);
        // for (int i=0; i<commandsNo; i++) {
        //     printf("\tCOMMANDS[%d] = %s; ",i,commands[i]);
        // }
        // printf("\n");

        int pendingWaits = 0; // hijos a esperar al salir del bucle commandsNo
        pid_t pid; /* Usado en el proceso padre para guardar el PID del proceso hijo */
        // SE EJECUTAN TODOS LOS COMANDOS QUE SE HAYAN PROCESADO
        printf("commands no = %i\n",commandsNo);
        for (int i=0; i<commandsNo; i++) {
            // Siempre y cuando haya freeThreads, si no -> wait for free threads = un hijo termina
            if (freeThreads == 0) {
                printf("waiting for free threads\n");
                if ((pid = wait(NULL)) == -1) {/* Espera a que termine el proceso hijo */
                    perror("wait()");
                    exit(EXIT_FAILURE);
                }
                printf("finalizó el hijo con PID = %i\n", pid);
                freeThreads++;
                pendingWaits--;
            }
            switch (pid = fork()) {
                case -1: /* fork() falló */
                    perror("fork()");
                    exit(EXIT_FAILURE);
                    break;
                case 0:                             /* Ejecución del proceso hijo tras fork() con éxito */
                    printf("EXECUTING command %i: %s\n",absCommandNo,commands[i]);
                    execute(commands[i]);
                    perror("execv()"); /* Esta línea no se debería ejecutar si la anterior tuvo éxito */
                    exit(EXIT_FAILURE);
                default:                  /* Ejecución del proceso padre tras fork() con éxito */
                    printf("CHILD %i CREATED\n",pid);
                    absCommandNo++;
                    freeThreads--;
                    pendingWaits++;
                    // wait(NULL);
                    break;
            }
        }
        printf("pendingWatis = %i\n",pendingWaits);
        // ESPERAMOS A LOS HIJOS PENDIENTES
        for (int i=0; i<pendingWaits; i++) {
            if ((pid = wait(NULL)) == -1) {/* Espera a que termine el proceso hijo */
                perror("wait()");
                exit(EXIT_FAILURE);
            }
            printf("finalizó el hijo con PID = %i\n", pid);
            freeThreads++;
        }
        // LIBERAMOS STRINGS DE COMANDOS
        for (int i=0; i<commandsNo; i++) {
            free(commands[i]);
        }
    }

    printf("%i commands executed\n",absCommandNo);

    free(commands);
    free(bufferAcumulativo);

    printf("BYE, BYE!!!\n");

    return EXIT_SUCCESS;
}


/***********************/
/* FUNCIONES AUXILIARES*/
/***********************/

int processArguments(int argc, char **argv) {
    int opt, nProc;

    optind = 1; // set optind to 1, analizamos desde ahí
    // esperamos 1 argumento o ninguno
    // si hay argumento debe llevar un número entre 1 y 8 detrás
    while ((opt = getopt(argc, argv, ":p:")) != -1)
    {
        switch (opt) {
        case 'p':
            nProc = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Uso: %s [-t] [-x NUMERO] [-s CADENA] [-y NUMERO]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    //sabemos si hay argumentos de más en esta condición ya que sale del while cuando no hay mas argumentos definidos en getopt con -
    if (optind<argc) {
        fprintf(stderr, "Uso: %s [-t] [-x NUMERO] [-s CADENA] [-y NUMERO]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (optind>3) {
        fprintf(stderr, "parámetro o argumentos de más");
        exit(EXIT_FAILURE);
    }
    // TODO - ver por qué no coge entre 1 y 8 procesos
    if (nProc > 8 || nProc < 1) {
        printf("executing con %i procesos\n",nProc);
        fprintf(stderr,"Error: El número de procesos en ejecución tiene que estar entre 1 y 8.");
        exit(EXIT_FAILURE);
    }
    
    return nProc;
}

// TODO - no se libera la memoria dinámica, después del execvp no regresa si hay éxito.
// también es cierto que cuando finaliza un proceso, se libera toda su memoria
int execute(char *command) {
    char **tokens = (char **)malloc(128*sizeof(char *));    //TODO analizar cuál sería el número máximo de argumentos
    int argc = splitCommand(command,tokens);

    // tokens[argc++] = (char *)malloc(1);
    tokens[argc++] = NULL;    // execvp espera un array NULL TERMINATED

    printf("ARGSV = %i\n\t",argc);
    for (int i=0; i<argc; i++) {
        printf("%s; ",tokens[i]);
    }
    printf("\n");

    // printf("\tCHILD EXECUTING %s\n",command);
    int statusCode = execvp(tokens[0], tokens);
    if (statusCode == -1) {
        fprintf(stderr,"COMMAND %s EXECUTION FAILED",command);
    }


    for (int i=0; i<argc; i++) {
        free(tokens[i]);
    }
    free(tokens);

    exit(EXIT_SUCCESS);
}

//TODO procesar varios espacios
int splitCommand(char *command, char *tokens[]) {
    printf("splitting command:'%s'\n",command);
    int tokensNo = 0;
    int tokenLength = 0;
    int relativeStartPosition = 0;
    // printf("CHILD processing %s\n",command);

    // iteramos sobre la entrada. 
    //  - Si es un espacio -> nuevo token a guardar en el array
    //  - Si es un NULL TERMINATING BYTE '\0' se sale del bucle. NOTA: hay que guardar este último token.
    for (int i=0; command[i]!='\0'; i++) {
        tokenLength++;
        printf("processing char '%c'\n",command[i]);
        // si es un espacio, se trata de un token -> se añade al array tokens
        if (command[i] == ' ') {
            // tokenLength = i-relativeStartPosition+1; //+1 para '\0'
            tokens[tokensNo] = (char *) malloc (tokenLength*sizeof(char));
            memcpy(tokens[tokensNo],(command+relativeStartPosition),tokenLength-1);
            tokens[tokensNo][tokenLength-1] = '\0'; // ADD TERMINATING NULL BYTE
            relativeStartPosition = i+1; // Actualizamos puntero; apunta al siguiente caracter al espacio 
            tokenLength = 0;
            tokensNo++;
        }

        /* CONDICIÓN DE PARADA PARA QUE NO ENTRE EN BUCLE INFINITO SI LA CADENA LLEGASE SIN EL '\0' */
        if (i==1000) {
            fprintf(stderr,"ERROR: string not null terminated\n");
            exit(EXIT_FAILURE);
        }
    }
    // a diferencia del splitInputInCommands, en este caso el '\0' determina el fin del último token, por tanto, hay que guardarlo fuera del bucle
    // atualizar tokensNo
    tokens[tokensNo] = (char *) malloc (tokenLength*sizeof(char));
    memcpy(tokens[tokensNo],(command+relativeStartPosition),tokenLength); // en este caso tokenLength no avanzó hasta '\0'
    tokensNo++;

    return tokensNo;
}

void checkLineSize(int totalBytesReadPerLine) {
    if (totalBytesReadPerLine > MAX_LINE_SIZE) {
        fprintf(stderr,"%s %i\n",ERR_LINE_SIZE,MAX_LINE_SIZE);
        exit(EXIT_FAILURE);
    }
}

// RETURNS
    // char * - string con el nuevo INPUT. Si el INPUT que llega no termina en '\n', probablemente quedará entrada por leer y procesar
// PARAMS
    // char *input   - puntero al string a procesar (será modificado en el memcpy)
    // int inputLength   - longitud de la entrada  (sin TERMINATING NULL BYTE, este se añade luego)
                        // NOTAS: no tiene porque ser la longitud a procesar, puede haberse procesado part del input en la llamada anterior a este método
                        // startOfNextCommand apunta al valor de length (dejo startOfNextCommand por legibilidad del código)
    // char **commands - puntero a strings donde se almacenan las órdenes (líneas) encontradas
    // int *startOfNextCommand - puntero que apunta a variable length del main. se pasa puntero para que la modificación de su valor dentro de la función (paso por referencia)
    // int *startOfUnprocessedChars - puntero a variable del main que se actualiza a la posición del primer carácter no procesado.
char * splitInputInCommands(char *input, int inputLength, char *commands[], int *startOfNextCommand, int *startOfUnprocessedChars, int *commandsNo) {
    input[inputLength] = '\0';

    // printf("Processing INPUT: '%s';strlen=%li\n----------------------------------\n",input,strlen(input));
    // printf("\tRemaining INPUT To Process:\n%s; strlen=%li\n",input+*startOfUnprocessedChars,strlen(input+*startOfUnprocessedChars));

    int relativeStartPosition = 0;
    int iNextChar;        //iterador que apunta al carácter analizado
    char *nextCommand = input+relativeStartPosition;
    *commandsNo = 0;
    // se itera sobre el input para guardar los comandos en un array.
    // Se comienza en startOfUnprocessedChars para no examinar entrada ya procesada en la llamada anterior a splitInputInCommands
    for (iNextChar=*startOfUnprocessedChars; input[iNextChar] != '\0'; iNextChar++) {
        // printf("char %c - %d\n",input[iNextChar],input[iNextChar]);
        // si es un salto de línea, se trata de un comando -> se añade al array commands
        if ((input[iNextChar] == '\n')) {
            nextCommand = input+relativeStartPosition;
            // si el comando es un salto de línea, no lo añadimos al array
            if (*nextCommand == '\n') {
                continue;
            }
            int commandStrLength = iNextChar-relativeStartPosition+1; //+1 para '\0'
            commands[*commandsNo] = (char *) malloc (commandStrLength*sizeof(char));
            // sustituimos el salto del línea por el NULL TERMINATING BYTE
            memcpy(commands[*commandsNo],nextCommand,commandStrLength-1);
            commands[*commandsNo][commandStrLength-1] = '\0'; // ADD TERMINATING NULL BYTE
            relativeStartPosition = iNextChar+1; // Actualizamos punteros; ambos apuntan al siguiente carácter al NEW_LINE
            (*commandsNo)++;
        }
        nextCommand = input+relativeStartPosition;
    }
    // printf("COMMANDS NO = %i\n",*commandsNo);
        
    char *newBufferAcumulativo = (char *)malloc((MAX_LINE_SIZE+1)*sizeof(char));      // MAX_LINE_SIZE + '\0'

    // si no se han encontrado órdenes:
    //  - se devuelve el input (bufferAcumulativo) tal cual. Tiene que mantener los mismos datos que tenía para intentar completar una orden con más datos de la entrada (read())
    //  - hay que actualizar la variable startOfNextCommand para no machacar los datos válidos con el siguiente read
    if (*commandsNo == 0) {
        *startOfNextCommand = inputLength+1;
    }
    // en otro caso, modificamos bufferAcumulativo copiando los datos no procesados y actualizamos startOfNextCommand y startOfUnprocessedChars
    else {
        // si se han encontrado órdenes, pero el último carácter era un salto de línea, no hay que crear nuevo bufferAcumulativo (input) ya que se ha procesado todo el input
        if (input[iNextChar-1] == '\n') {
            *startOfNextCommand = 0;
            // printf("INPUT totalmente procesado\n");
        }
        // copiamos los bytes (caracteres) desde el carácter siguiente al NEW_LINE hasta el final de entrada (antes del '\0')
        else if (relativeStartPosition < inputLength) {
            newBufferAcumulativo = memmove(newBufferAcumulativo,nextCommand,inputLength+1); // +1 - copio también el '\0' para imprimirlo correctamente en el main
            // newBufferAcumulativo[inputLength+1] = '\0';
            free(input);
            *startOfNextCommand = inputLength-relativeStartPosition; // con esto devolvemos el carácter siguiente a donde terminó la última orden, eg:
                    // string 'ls -l\necho next\n' devuelve la posición de la e del echo. Queremos que devuelva 
                    // para reads de 16 bytes con la entrada 'ps\necho luego\nls -lnext\n', a splitInputInCommands le llega INPUT = 'ps\necho luego\nls'
                    // crea dos órdenes, a saber, 'ps' y 'echo luego', pero no ls, ya que falta el ' -l \n'. Recordemos que no crea orden hasta que encuentra '\n'
                    // No quiero que vuelva a procesar el ls ya que ya se había procesado, y en INPUTS muy grandes se perdería mucha eficiencia.
                    // de ahí que se devuelva otra
            *startOfUnprocessedChars = inputLength+1-iNextChar+1; // inputLength+1 -> '\n' // iNextChar+1 -> si hemos procesado 3 caracteres => el siguiente a procesar es el 4
            // printf("\tbufferBackToMain = %s;\tstartOfUnprocessedChars=%d\n",newBufferAcumulativo,*startOfUnprocessedChars);
        }
    }

    return newBufferAcumulativo;
}


void processLineCommand(char *stringRead) {
    exit(EXIT_SUCCESS);
}

