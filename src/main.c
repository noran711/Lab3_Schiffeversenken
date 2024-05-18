#include <stm32f0xx.h>
#include "mci_clock.h"
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define DEBUG

// This is a simple macro to print debug messages if DEBUG is defined
#ifdef DEBUG
  #define LOG( msg... ) printf( msg );
#else
  #define LOG( msg... ) ;
#endif

// Select the Baudrate for the UART
#define BAUDRATE 9600

#define BOARD_SIZE 10


// Maximal mögliche Länge der Nachricht (einschließlich Nullterminator)
#define MAX_MESSAGE_LENGTH 15 


// For supporting printf function we override the _write function to redirect the output to UART
int _write( int handle, char* data, int size ) {
    int count = size;
    while( count-- ) {
        while( !( USART2->ISR & USART_ISR_TXE ) ) {};
        USART2->TDR = *data++;
    }
    return size;
}


// Funktion zur Generierung eines zufälligen Spielfelds mit den gegebenen Schiffen
void generate_field(int field[10][10]) {
    // Initialisiere das Spielfeld mit 0 (kein Schiff)
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            field[i][j] = 0;
        }
    }

    // Setze die Schiffe auf das Spielfeld
    int ships[] = {5, 4, 4, 3, 3, 3, 2, 2, 2, 2}; // Größe der Schiffe
    srand(time(NULL));
    for (int s = 0; s < 10; ++s) {
        int ship_size = ships[s];
        // Wähle zufällige Startposition und Orientierung für das Schiff

        int row = rand() % 10;
        int col = rand() % 10;
        int horizontal = rand() % 2; // 0 für horizontal, 1 für vertikal
        // Überprüfe, ob das Schiff an der gewählten Position platziert werden kann
        int valid_position = 1;
        if (horizontal) {
            if (col + ship_size > 10) {
                valid_position = 0;
            } else {
                for (int i = col; i < col + ship_size; ++i) {
                    if (field[row][i] != 0) {
                        valid_position = 0;
                        break;
                    }
                }
            }
        } else {
            if (row + ship_size > 10) {
                valid_position = 0;
            } else {
                for (int i = row; i < row + ship_size; ++i) {
                    if (field[i][col] != 0) {
                        valid_position = 0;
                        break;
                    }
                }
            }
        }
        // Platziere das Schiff auf dem Spielfeld, wenn die Position gültig ist
        if (valid_position) {
            if (horizontal) {
                for (int i = col; i < col + ship_size; ++i) {
                    field[row][i] = ship_size;
                }
            } else {
                for (int i = row; i < row + ship_size; ++i) {
                    field[i][col] = ship_size;
                }
            }
        } else {
            // Versuche ein anderes Schiff zu platzieren, wenn die Position ungültig ist
            s--;
        }
    }
}

// Funktion zur Berechnung der Spielfeldchecksumme und Erstellung der Nachricht
void calculate_checksum(int field[10][10], char checksum_msg[]) {
    // Initialisierung der Prüfsumme-Nachricht mit 'CS'
    checksum_msg[0] = 'C';
    checksum_msg[1] = 'S';

    // Zähler für die Position im checksum_msg
    int msg_pos = 2;

    // Zähle die Schiffe in jeder Spalte und füge sie der Nachricht hinzu
    for (int col = 0; col < 10; ++col) {
        int ships_count = 0;
        for (int row = 0; row < 10; ++row) {
            ships_count += (field[row][col] > 0); // Zähle die Schiffe in der aktuellen Spalte
        }
        // Füge die Anzahl der Schiffe der Nachricht hinzu
        checksum_msg[msg_pos++] = (char)(ships_count + '0');
    }

    // Füge den Zeilenumbruch hinzu
    checksum_msg[msg_pos] = '\n';
    checksum_msg[msg_pos + 1] = '\0'; // Nullterminierung der Zeichenkette
}

// Funktion zur Extraktion der Trefferzahlen aus der empfangenen Checksumme
void extract_hit_counts(const char* checksum_msg, int* hit_counts) {
    // Die ersten zwei Zeichen überspringen ('CS')
    for (int i = 2; i < 12; ++i) {
        hit_counts[i - 2] = checksum_msg[i] - '0';
    }
}

// Funktion zum Sortieren der Spalten basierend auf der Anzahl der Treffer
void sort_columns_by_hits(int* hit_counts, int* sorted_columns) {
    for (int i = 0; i < BOARD_SIZE; ++i) {
        sorted_columns[i] = i;
    }

    // Einfaches Bubblesort zur Sortierung der Spalten nach der Anzahl der Treffer
    for (int i = 0; i < BOARD_SIZE - 1; ++i) {
        for (int j = 0; j < BOARD_SIZE - i - 1; ++j) {
            if (hit_counts[j] < hit_counts[j + 1]) {
                // Spalten-Indizes tauschen
                int temp = sorted_columns[j];
                sorted_columns[j] = sorted_columns[j + 1];
                sorted_columns[j + 1] = temp;
                // Anzahl der Treffer tauschen
                temp = hit_counts[j];
                hit_counts[j] = hit_counts[j + 1];
                hit_counts[j + 1] = temp;
            }
        }
    }
}

// Function to add a small delay
void delay(uint32_t milliseconds) {
    // Assuming a clock frequency of 48 MHz
    for (uint32_t i = 0; i < milliseconds * 48000; ++i) {
        __NOP();
    }
}



int main(void){
    // Configure the system clock to 48MHz
    EPL_SystemClock_Config();

    GPIOC->MODER &= ~GPIO_MODER_MODER13;
    GPIOC->MODER |= GPIO_MODER_MODER13_0;
    GPIOC->MODER &= ~GPIO_MODER_MODER13_Msk;

    // Enable peripheral GPIOA clock
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    // Enable peripheral USART2 clock
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // Configure PA2 as USART2_TX using alternate function 1
    GPIOA->MODER |= GPIO_MODER_MODER2_1;
    GPIOA->AFR[0] |= 0b0001 << (4*2);

    // Configure PA3 as USART2_RX using alternate function 1
    GPIOA->MODER |= GPIO_MODER_MODER3_1;
    GPIOA->AFR[0] |= 0b0001 << (4*3);

    // Configure the UART Baud rate Register 
    USART2->BRR = (APB_FREQ / BAUDRATE);
    // Enable the UART using the CR1 register
    USART2->CR1 |= ( USART_CR1_RE | USART_CR1_TE | USART_CR1_UE );

    // Configure LED pin
    GPIOA->MODER |= GPIO_MODER_MODER5_0; // Set PA5 as output

    // Configure C13 button pin
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN; // Enable GPIOC clock
    GPIOC->MODER &= ~(GPIO_MODER_MODER13); // Clear MODER13 bits for input mode

    uint8_t rxb = '\0';

    // Variable to track the game state
    enum GameState {
        WAITING_FOR_START,
        WAITING_FOR_CHECKSUM,
        GENERATING_FIELD,
        PLAYER_1,
        PLAYER_2,
        WAITING_FOR_START_MESSAGE,
    }; enum GameState GameState = WAITING_FOR_START;

    int spieler;
    char start[15] = {0};
    char start_1[15] = {0};
    char checksum_g[15] = {0};
    int field[10][10];
    char checksum[15] = {0};
    int message_length = 0; // Länge der bisher empfangenen Nachricht
    int message_l_checksum = 0;
    int message_l = 0;
    // Array zur Speicherung der Anzahl der Treffer in den Spalten
    int hit_counts[BOARD_SIZE] = {0};
    // Array zur Speicherung der sortierten Spalten-Indizes
    int sorted_columns[BOARD_SIZE] = {0};
    // Globale Variable zur Speicherung der Checksumme des Gegners
    char opponent_checksum[15] = {0};

    bool schiessen = false;
    bool beschossen_werden = false;
    bool treffer = false;
    bool kein_treffer = false;
    bool schuss_gesendet = false;

    int meine_treffer = 0;
    int treffer_g = 0;
    int meine_shots = 0;
    int shots_g = 0;

    char nachricht[1] = {0};


    for(;;){

        // Wait for the data to be received
        //while( !( USART2->ISR & USART_ISR_RXNE ) );

        // Read the data from the RX buffer
        //rxb = USART2->RDR;

        // Print the received data to the console
        //LOG("[DEBUG-LOG]: %d\r\n", rxb );
        //GameState = WAITING_FOR_START;
        
        switch(GameState){

            case WAITING_FOR_START:
                // Überprüfe den Startknopf
                if ((GPIOC->IDR & GPIO_IDR_13) == 0) {
                    // Startnachricht senden und Spielzustand ändern
                    LOG("START11928041\n");
                    spieler = 1;
                    GameState = WAITING_FOR_CHECKSUM;
                    break;
                } 
                if (USART2->ISR & USART_ISR_RXNE) {
                     char received_char = USART2->RDR;
                        start[message_length] = received_char;
                        message_length++;
                        
                        // Überprüfe, ob das letzte empfangene Zeichen '\n' ist
                        if (received_char == '\n') {
                            spieler = 2; // Spieler auf 2 setzen
                            GameState = GENERATING_FIELD; // Spielzustand entsprechend setzen
                            message_length = 0;
                            break;
                            
                        }
                    }
                break; 

                            
            

            case WAITING_FOR_CHECKSUM:
                // Check for incoming messages
                if (USART2->ISR & USART_ISR_RXNE) {
                        char received_c = USART2->RDR;
                        checksum_g[message_l_checksum] = received_c;
                        message_l_checksum++;
                        // Check for checksum message
                        if (received_c == '\n') {
                            checksum_g[message_l_checksum] = '\0'; // Nullterminator hinzufügen
                            //LOG("%s", checksum_g); // Ausgabe der gespeicherten Checksumme
                            message_l_checksum = 0; // Zurücksetzen des Zählers
                        if (spieler == 2){
                                LOG("START11928041\n");
                                GameState = PLAYER_2;
                                break;
                            }
                        else if(spieler == 1){
                        GameState = GENERATING_FIELD;
                        break;
                        }
                }
                }
                break;

            case GENERATING_FIELD:
                // Generate the game board
                generate_field(field);
                //Calculate checksum
                calculate_checksum(field, checksum);
                //send checksum
                LOG("%s", checksum);
                if(spieler == 1){
                    GameState = WAITING_FOR_START_MESSAGE;
                    break;
                }
                if(spieler == 2){
                    GameState = WAITING_FOR_CHECKSUM;
                    break;
                }
                break;

            case WAITING_FOR_START_MESSAGE:
                //check for incoming messages
                if (USART2->ISR & USART_ISR_RXNE) {
                     char received_ch = USART2->RDR;
                        start_1[message_length] = received_ch;
                        message_length++;
                        //LOG("%s", start_1);
                        // Überprüfe, ob das letzte empfangene Zeichen '\n' ist
                        if (received_ch == '\n') {
                            
                            GameState = PLAYER_1; // Spielzustand entsprechend setzen
                            message_length = 0;
                            break;  
                        }
                    }
                break;


            case PLAYER_1:
                // Game logic
                
                // Extrahiere die Anzahl der Treffer aus der Checksumme
                extract_hit_counts(checksum_g, hit_counts);

                // Sortiere die Spalten basierend auf der Anzahl der Treffer
                sort_columns_by_hits(hit_counts, sorted_columns);

                if(schiessen){
                    if(treffer){
                        // Feld neben treffer finden
                        // überprüfen ob ich schon mal dahingeschossen habe
                        //Nachricht senden Bsp. BOOM09\n
                        meine_shots++;
                        treffer = false;
                    }
                    if(kein_treffer){
                        // erste Spalte bzw. spalte für spalte von sorted_columns zufällige Reihe
                        // überprüfen ob schon dorthin geschossen wurde
                        // Nachricht senden bsp. BOOM09\n
                        meine_shots++;
                        kein_treffer = false;
                    }
                    schuss_gesendet = true;
                    
                }

                if(schuss_gesendet){
                    if (USART2->ISR & USART_ISR_RXNE) {
                     char received_m = USART2->RDR;
                        nachricht[message_l] = received_m;
                        message_l++;
                        // Überprüfe, ob das letzte empfangene Zeichen '\n' ist
                        if (received_m == '\n') {
                            if(nachricht[0] == 'T'){
                                meine_treffer++;
                                treffer = true;
                            }
                            if(nachricht[0] == 'W'){
                                kein_treffer = true;
                            }
                            beschossen_werden = true;
                        }
                    }
                }

                if(beschossen_werden){

                }


                
                
                break;

            
        }




    
        }
    }

