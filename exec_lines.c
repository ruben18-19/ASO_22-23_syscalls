#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h> //printf
#include <unistd.h> //read, write, STDIN_FILENO
#include <string.h> // memcpy



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

void    checkLineSize(int);
int     splitInCommands(char *,int,char **,int *,int *);
void    processLineCommand(char *);


int main(int argc, char *argv[]) {

    // printf("flag: %d, n: %d, s: \"%s\"", flag, n, s);
    // for (int i = optind; i < argc; i++)
    //     printf(", param%d: \"%s\"", i - optind, argv[i]);
    // printf("\n");

    // char bufferAcumulativo[129] = "";      // MAX_LINE_SIZE + '\0'
    char *bufferAcumulativo = (char *)malloc((MAX_LINE_SIZE+1)*sizeof(char));      // MAX_LINE_SIZE + '\0'
    char buffer[16] = "";
    ssize_t bytesRead = 0;
    ssize_t totalBytesReadPerLine = 0;
    // char *lines[MAX_LINES_IN_BUFFER];
    char **lines = (char **)malloc(MAX_LINES_IN_BUFFER*sizeof(char *));     //reservamos espacio para MAX_LINES_IN_BUFFER char * (strings)
    // for (int i=0; i<MAX_LINES_IN_BUFFER; i++) {
    //     lines[i] = (char *)malloc((MAX_LINE_SIZE+1)*sizeof(char));      //una orden podría ocupar 128 bytes + '\0' (se usa en splitInCommands). En ese caso no se usarán más strings, sólo el 0
    // }
    int startOfNextOrder = 0;      // indica hasta qué posición del buffer hay carácteres válidos
    // este puntero se va moviendo y siempre apunta al comienzo de la siguiente orden
                                    // es decir, a bufferAcumulativo[0].
                                    // hay que actualizar bufferAcumulativo después de cada llamada a fork
                                    // antes de cada llamada a fork(), hay que hacer una copia de las órdenes para
    int startOfUnprocessedChars = 0;
    int currentPosition = 0;
    int lineFound = 0;
    int round = 0;

    while ((bytesRead = read(STDIN_FILENO,buffer,MAX_BYTES_PER_READ))) {
        // ERROR READING
        if (bytesRead == -1) {
            perror("read()");
            exit(EXIT_FAILURE);
        }
        
        printf("ROUND %d\n",round++);

        // si superamos el límite de 128 bytes/línea => ERR; se sale con EXIT_FAILURE dentro de la función
        checkLineSize(totalBytesReadPerLine += bytesRead);

        // copiamos en bufferAcumulativo los bytes leídos en esta iteración
        // puede que haya datos válidos en este buffer, esto está indicado por startOfNextOrder
        // printf("bufferAcumulativo=%s; buffer=%s\n",bufferAcumulativo,buffer);
        memcpy(bufferAcumulativo+startOfNextOrder,buffer,bytesRead);
        // printf("bufferAcumulativo=%s; buffer=%s\n",bufferAcumulativo,buffer);

        startOfNextOrder += bytesRead;
        int ordenesNo = splitInCommands(bufferAcumulativo, startOfNextOrder, lines, &startOfNextOrder, &startOfUnprocessedChars);
        for (int i=0; i<ordenesNo; i++) {
            printf("\tCOMMANDS[%d] = %s; ",i,lines[i]);
        }
        printf("\n");

        // se buscan todas las órdenes que haya en bufferAcumulativo
        // por definición del problema, una orden es la ristra de caracteres antes de un salto de línea (de ahí el nombre de la función)
        // bufferAcumulativo se actualiza dentro de esta función si se encuentran órdenes
        // if (!splitInCommands(&bufferAcumulativo,bytesRead,lines,&startOfNextOrder)) {
        //     continue; // no hay salto de línea, continuamos (las variables se han actualizado en la llamada)
        // }
        // else {
        //     printf("%zu bytes read: vamos a procesar el comando:\n",totalBytesReadPerLine); // %s");
        //     // posibilidad e hacer fork y que el hijo procese la línea y validación. creo que más correcto
        //     // processLineCommand(buf);  //procesar línea, el hijo hace fork y ejecuta orden
        //     bytesRead = 0; totalBytesReadPerLine = 0;
        // }
        
    }
        
    // printf("%zu bytes read: %s\n",bytesRead,buf);

    

    return EXIT_SUCCESS;
}


/***********************/
/* FUNCIONES AUXILIARES*/
/***********************/

void checkLineSize(int totalBytesReadPerLine) {
    if (totalBytesReadPerLine > MAX_LINE_SIZE) {
        fprintf(stderr,"%s %i\n",ERR_LINE_SIZE,MAX_LINE_SIZE);
        exit(EXIT_FAILURE);
    }
}

// RETURNS
    // int - number of lines found
// PARAMS
    // char *input   - puntero al string a procesar (será modificado en el memcpy)
    // int length   - longitud de la entrada  (sin TERMINATING NULL BYTE, este se añade luego)
                        // NOTAS: no tiene porque ser la longitud a procesar, puede haberse procesado en la llamada anterior a este método parte del INPUT
                        // startOfNewOrder apunta al valor de length (dejo startOfNewOrder por legibilidad del código)
    // char **lines - puntero a strings donde se almacenan las órdenes (líneas) encontradas
    // int *startOfNewOrder - apunta a variable length del main. se pasa puntero para que la modificación de su valor dentro de la función (paso por referencia)
    // int *startOfUnprocessedChars - puntero a variable del main que se actualiza a la posición del primer carácter no procesado.
                        // el INPUT puede haber sido parcialmente procesado en la última llamada a esta función
int splitInCommands(char *input, int length, char *lines[], int *startOfNewOrder, int *startOfUnprocessedChars) {
    int inputLength = length;
    input[inputLength] = '\0';

    printf("\tProcessing INPUT:\n%s; strlen=%li\n",input,strlen(input));
    printf("\tRemaining INPUT To Process:\n%s; strlen=%li\n",input+*startOfUnprocessedChars,strlen(input+*startOfUnprocessedChars));

    int linesNo = 0;
    int relativeStartPosition = 0;
    int processedChars;        //es el iterador que apunta al carácter analizado
    for (processedChars=*startOfUnprocessedChars; input[processedChars] != '\0'; processedChars++) {
        // si es un salto de línea, se trata de un comando -> se añade al array lines
        if ((input[processedChars] == '\n') || (input[processedChars] == '\r')) {
            int commandStrLength = processedChars-relativeStartPosition+1; //+1 para '\0'
            // printf("commandStrLength=%d\n",commandStrLength);
            lines[linesNo] = (char *) malloc (commandStrLength*sizeof(char));
            memcpy(lines[linesNo],(input+relativeStartPosition),commandStrLength-1);
            lines[linesNo][commandStrLength] = '\0'; // ADD TERMINATING NULL BYTE
            relativeStartPosition = processedChars+1; // Actualizamos punteros; ambos apuntan al siguiente carácter al NEW_LINE
            // printf("command[%i]=%s",linesNo,lines[linesNo]);
            linesNo++;
        }
    }

    // si no se han encontrado órdenes:
    //  - bufferAcumulativo no se modifica. Tiene que mantener los mismos datos que tenía para intentar completar una orden con más datos de la entrada (read())
    //  - hay que actualizar la variable startOfNewOrder para no machacar los datos válidos con el siguiente read
    if (linesNo == 0) {
        *startOfNewOrder = inputLength+1;
    }
    // en otro caso, modificamos bufferAcumulativo copiando los datos no procesados y actualizamos startOfNewOrder y startOfUnprocessedChars
    else {
        char *remainingStr = memcpy(input,input+relativeStartPosition,inputLength); // copiamos los bytes (caracteres) desde el carácter siguiente al NEW_LINE hasta el final de entrada (antes del '\0')
        *startOfNewOrder = inputLength-relativeStartPosition; // con esto devolvemos el carácter siguiente a donde terminó la última orden, eg:
                // string 'ls -l\necho next\n' devuelve la posición de la e del echo. Queremos que devuelva 
                // para reads de 16 bytes con la entrada 'ps\necho luego\nls -lnext\n', a splitInCommands le llega INPUT = 'ps\necho luego\nls'
                // crea dos órdenes, a saber, 'ps' y 'echo luego', pero no ls, ya que falta el ' -l \n'. Recordemos que no crea orden hasta que encuentra '\n'
                // No quiero que vuelva a procesar el ls ya que ya se había procesado, y en INPUTS muy grandes se perdería mucha eficiencia.
                // de ahí que se devuelva otra
        // *absoluteStartOfUnprocessedChars = inputLength-*startOfNewOrder;
        *startOfUnprocessedChars = inputLength+1-processedChars+1; // inputLength+1 -> '\n' // processedChars+1 -> si hemos procesado 3 caracteres => el siguiente a procesar es el 4
        // printf("\tRemainginStr = %s;\tstartOfUnprocessedChars=%d\n",remainingStr,*startOfUnprocessedChars);
    }

    return linesNo;
}


void processLineCommand(char *stringRead) {
    exit(EXIT_SUCCESS);
}

