#include <stm32f0xx.h>
#include "mci_clock.h"
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>


// spielfeldnachrichten einlesen und schauen ob schummeln
// überprüfen ob 30 treffer auch 30 verschiedene felder sind !
// schummeln !
// (intelligente schussstrategie erweitern)




#define DEBUG

#define LOGGING

// This is a simple macro to print debug messages if DEBUG is defined
#ifdef DEBUG
  #define GAME_LOG( msg... ) printf( msg );
#else
  #define GAME_LOG( msg... ) ;
#endif

#ifdef LOGGING
  #define LOG( msg... ) { \
      char buffer[256]; \
      int len = snprintf(buffer, sizeof(buffer), msg); \
      logging((uint8_t*)buffer, len); \
  }
#else
  #define LOG( msg... ) ;
#endif

int logging(uint8_t* msg, size_t size) {
    // Buffer for the new message with an additional character for '#'
    uint8_t new_msg[size + 2]; // +1 for '#' and +1 for null terminator

    // Add '#' at the beginning
    new_msg[0] = '#';

    // Copy the original message after it
    memcpy(new_msg + 1, msg, size);

    // Null-terminate the new message
    new_msg[size + 1] = '\0';

    // Print the new message (can be modified to store it elsewhere)
    printf("%s", new_msg);

    return 0; // Success
}




// Select the Baudrate for the UART
#define BAUDRATE 9600

#define BOARD_SIZE 10

#define ROWS 10
#define COLS 10


// Maximal mögliche Länge der Nachricht (einschließlich Nullterminator)
#define MAX_MESSAGE_LENGTH 15 

#define TIMEOUT 100000




// For supporting printf function we override the _write function to redirect the output to UART
int _write( int handle, char* data, int size ) {
    int count = size;
    while( count-- ) {
        while( !( USART2->ISR & USART_ISR_TXE ) ) {};
        USART2->TDR = *data++;
    }
    return size;
}

uint8_t timeout(uint32_t time){
    static uint32_t cnt = 0;
    cnt++;
    if (cnt > time){
        return 1;
    }
    return 0;
}

void error(void){
    for(;;){}
}

void ADC_Init(void) {
    // Enable the GPIOA and ADC clock
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN; 
    RCC->APB2ENR |= RCC_APB2ENR_ADCEN; 

    // Set the GPIOA pin 0 to analog mode
    GPIOA->MODER |= GPIO_MODER_MODER0;  

    // Set the ADC to continuous mode and select scan direction
    ADC1->CHSELR |= ADC_CHSELR_CHSEL0 ; 
    ADC1->CFGR1  |= ADC_CFGR1_CONT | ADC_CFGR1_SCANDIR;
    // Set Sample time 
    ADC1->SMPR   |= ADC_SMPR_SMP_0;

    // If ADC is not ready set the ADC ready bit
    if ((ADC1->ISR & ADC_ISR_ADRDY) != 0){   
        ADC1->ISR |= ADC_ISR_ADRDY; 
    }
    // Enable the ADC
    ADC1->CR |= ADC_CR_ADEN; 
    // Wait for the ADC to be ready
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0){
        if (timeout(TIMEOUT)){
            error();
        }
    }

    // Start the ADC
    ADC1->CR |= ADC_CR_ADSTART;
}

uint16_t ADC_Read(void) {
    while ((ADC1->ISR & ADC_ISR_EOC) == 0) {
        if (timeout(TIMEOUT)){
            error();
        }
    }
    return ADC1->DR;
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

        int row = ADC_Read() % 10;
        int col = ADC_Read() % 10;
        int horizontal = ADC_Read() % 2; // 0 für horizontal, 1 für vertikal
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

bool isShotAlreadyTaken(int shot, int takenShots[], int meine_shots){
    for (int i = 0; i < meine_shots; ++i){
        if(takenShots[i] == shot){
            return true;
        }
    }
    return false;
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

    ADC_Init();

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
        PLAYING,
        WAITING_FOR_START_MESSAGE,
        SEND_SF_MESSAGE,
    }; enum GameState GameState = WAITING_FOR_START;

    int spieler;
    char start[15] = {0};
    char start_1[15] = {0};
    char checksum_g[15] = {0};
    int field[ROWS][COLS];
    char checksum[15] = {0};
    int message_length = 0; // Länge der bisher empfangenen Nachricht
    int message_l_checksum = 0;
    int message_l = 0;
    int message_length_s = 0;
    int message_sf_length = 0;
    // Array zur Speicherung der Anzahl der Treffer in den Spalten
    int hit_counts[BOARD_SIZE] = {0};
    // Array zur Speicherung der sortierten Spalten-Indizes
    int sorted_columns[BOARD_SIZE] = {0};

    bool schiessen = false;
    bool beschossen_werden = false;
    bool treffer = false;
    bool kein_treffer = false;
    bool schuss_gesendet = false;

    int meine_treffer = 0;
    int treffer_g = 0;
    int meine_shots = 0;
    int shots_g = 0;
    int shot;
    int row;
    int col;

    char nachricht[16] = {0};
    char schuss_g[16] = {0};
    char message[16] = {0};

    int takenShots[100] = {0};
    int zaehler = 0;

    char sf_g[16] = {0};

    // Variablen zum Speichern der SF-Nachrichten
    char sf_messages[10][16];
    int sf_message_count = 0;
        


    for(;;){

        // Wait for the data to be received
        //while( !( USART2->ISR & USART_ISR_RXNE ) );

        // Read the data from the RX buffer
        //rxb = USART2->RDR;

        // Print the received data to the console
        //LOG("[DEBUG-LOG]: %d\r\n", rxb );
        //GameState = WAITING_FOR_START;
        
        switch(GameState) {
            case WAITING_FOR_START:
                // Check the start button
                if ((GPIOC->IDR & GPIO_IDR_13) == 0) {
                    // Send start message and change game state
                    LOG("button pressed\n");
                    GAME_LOG("START11928041\n");
                    
                    spieler = 1;
                    GameState = WAITING_FOR_CHECKSUM;
                    break;
                } 
                if (USART2->ISR & USART_ISR_RXNE) {
                    char received_char = USART2->RDR;
                    start[message_length] = received_char;
                    message_length++;
                                
                    // Check if the last received character is '\n'
                    if (received_char == '\n') {
                        LOG("Received: %s", start);
                        spieler = 2; // Set player to 2
                        GameState = GENERATING_FIELD; // Set game state accordingly
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
                        checksum_g[message_l_checksum] = '\0'; // Add null terminator
                        LOG("Received checksum: %s", checksum_g);
                        message_l_checksum = 0; // Reset the counter
                        if (spieler == 2) {
                            GAME_LOG("START11928041\n");
                            
                            kein_treffer = true;
                            beschossen_werden = true;
                            GameState = PLAYING;
                            break;
                        } else if (spieler == 1) {
                            GameState = GENERATING_FIELD;
                            break;
                        }
                    }
                }
                break;

            case GENERATING_FIELD:
                // Generate the game board
                generate_field(field);
                //LOG("Field generated");

                // Calculate checksum
                calculate_checksum(field, checksum);
                LOG("Checksum calculated: %s", checksum);

                
                // Send checksum
                GAME_LOG("%s", checksum);
                

                if (spieler == 1) {
                    
                    GameState = WAITING_FOR_START_MESSAGE;
                    break;
                }
                if (spieler == 2) {
                
                    GameState = WAITING_FOR_CHECKSUM;
                    break;
                }
                break;

            case WAITING_FOR_START_MESSAGE:
                // Check for incoming messages
                if (USART2->ISR & USART_ISR_RXNE) {
                    char received_ch = USART2->RDR;
                    start_1[message_length] = received_ch;
                    message_length++;
                    // Check if the last received character is '\n'
                    if (received_ch == '\n') {
                        LOG("Received: %s", start_1);
                        kein_treffer = true;
                        schiessen = true;
                        message_length = 0;
                        GameState = PLAYING; // Set game state accordingly
                        break;  
                    }
                }
                break;

            case PLAYING:
                // Game logic

                // Extract hit counts from the checksum
                extract_hit_counts(checksum_g, hit_counts);
               

                // Sort columns based on hit counts
                sort_columns_by_hits(hit_counts, sorted_columns);
                
                

                while (meine_treffer < 30 && treffer_g < 30 && meine_shots <= 100 && shots_g <= 100) {
                    if (schiessen) {
                        if (treffer) {
                            // Find adjacent field
                            int shot_found = 0;
                            for (int dir = 0; dir < 4 && !shot_found; ++dir) {
                                int new_row = row, new_col = col;
                                switch (dir) {
                                    case 0: new_row = row + 1; break;
                                    case 1: new_row = row - 1; break;
                                    case 2: new_col = col + 1; break;
                                    case 3: new_col = col - 1; break;
                                }
                                shot = new_col * 10 + new_row;
                                if (new_row >= 0 && new_row < 10 && new_col >= 0 && new_col < 10 &&
                                    !isShotAlreadyTaken(shot, takenShots, meine_shots)) {
                                    LOG("Shot taken at %d,%d\n", new_col, new_row);
                                    GAME_LOG("BOOM%d%d\n", new_col, new_row);
                                    
                                    takenShots[meine_shots++] = shot;
                                    shot_found = 1;
                                }
                            }
                            if (!shot_found) {
                                kein_treffer = true;
                                treffer = false;
                            } else {
                                treffer = false;
                            }
                            schuss_gesendet = true;
                            schiessen = false;
                        }
                        if (kein_treffer) {
                            // Search first column or column by column from sorted_columns for random row
                            bool treffer_gefunden = false;
                            for (int i = 0; i < COLS && !treffer_gefunden; ++i) {
                                col = sorted_columns[i];
                                for (int j = 0; j < ROWS; ++j) {
                                    shot = col * 10 + j;
                                    if (!isShotAlreadyTaken(shot, takenShots, meine_shots)) {
                                        LOG("Shot taken at %d,%d\n", col, j);
                                        GAME_LOG("BOOM%d%d\n", col, j);
                                        
                                        takenShots[meine_shots++] = shot;
                                        kein_treffer = false;
                                        treffer_gefunden = true;
                                        break;
                                    }
                                }
                            }
                            schuss_gesendet = true;
                            schiessen = false;
                        }
                    }

                    if (schuss_gesendet) {
                        if (USART2->ISR & USART_ISR_RXNE) {
                            char received_m = USART2->RDR;
                            nachricht[message_l] = received_m;
                            message_l++;

                            // Check if the last received character is '\n'
                            if (received_m == '\n') {
                                nachricht[message_l] = '\0'; // Null-terminate the string
                                char first_char = nachricht[0];
                                char second_char = nachricht[1];

                                // Check if the first character is 'T' or 'W'
                                if (first_char == 'T') {
                                    meine_treffer++;
                                    
                                    treffer = true;
                                } else if (first_char == 'W') {
                                    
                                    kein_treffer = true;
                                } else if (first_char == 'S' && second_char == 'F') {
                                    LOG("SF message received, We won!\n");
                                    GameState = SEND_SF_MESSAGE;
                                    break;
                                }

                                message_l = 0;
                                memset(nachricht, 0, sizeof(nachricht));
                                schiessen = false;
                                beschossen_werden = true;
                                schuss_gesendet = false;
                            }
                        }
                    }

                    if (beschossen_werden) {
                        if (USART2->ISR & USART_ISR_RXNE) {
                            char received = USART2->RDR;

                            schuss_g[message_length_s] = received;
                            message_length_s++;

                            // Check if the last received character is '\n'
                            if (received == '\n') {
                                schuss_g[message_length_s] = '\0';  // Null-terminate the string

                                // Extract row and column information from schuss_g
                                int r = schuss_g[5] - '0';
                                int c = schuss_g[4] - '0';
                                char first_c = schuss_g[0];
                                char second_c = schuss_g[1];

                                if(treffer){
                                    LOG("We hit at: %d\n", shot);
                                }
                                if(kein_treffer){
                                    LOG("We miss at: %d\n", shot);
                                }

                                if (first_c == 'S' && second_c == 'F') {
                                    LOG("SF message received, We won!\n");
                                    GameState = SEND_SF_MESSAGE;
                                    break;
                                } else {
                                    if (field[r][c] > 0) {
                                        treffer_g++;
                                        if (treffer_g == 30) {
                                            LOG("30 hits received, We lost!\n");
                                            GameState = SEND_SF_MESSAGE;
                                            break;
                                        }
                                        LOG("Hit received at %d,%d\n", c, r);
                                        GAME_LOG("T\n");
                                        
                                    } else {
                                        LOG("Miss received at %d, %d\n", c, r);
                                        GAME_LOG("W\n");
                                        
                                    }
                                    shots_g++;
                                }

                                message_length_s = 0;  // Reset message_length_s for the next message
                                memset(schuss_g, 0, sizeof(schuss_g));
                                schiessen = true;
                                beschossen_werden = false;
                            }
                        }
                    }
                }
                break;

            case SEND_SF_MESSAGE:
                for (int col = 0; col < COLS; ++col) {
                    // Construct the SF message for each column
                    char sf_message[15];
                    sf_message[0] = 'S';
                    sf_message[1] = 'F';
                    sf_message[2] = (char)(col + '0'); // Convert column number to char
                    sf_message[3] = 'D';
                    
                    // Fill in the ship positions for each row in the current column
                    for (int row = 0; row < ROWS; ++row) {
                        sf_message[row + 4] = (char)(field[row][col] + '0'); // Convert ship size to char
                    }
                    
                    sf_message[14] = '\n';
                    sf_message[15] = '\0'; // Null-terminate the string

                    GAME_LOG("%s", sf_message);
                    
                }
                
                LOG("Game end\n");
                GameState = WAITING_FOR_START;
                break;

            



}
    }
}