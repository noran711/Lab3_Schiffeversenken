#include <stm32f0xx.h>
#include "mci_clock.h"
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>


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
    // Puffer für die neue Nachricht mit einem zusätzlichen Zeichen für '#'
    uint8_t new_msg[size + 2]; // +1 für '#' und +1 für Null-Terminierung

    // Füge '#' am Anfang hinzu
    new_msg[0] = '#';

    // Kopiere die ursprüngliche Nachricht danach
    memcpy(new_msg + 1, msg, size);

    // Null-terminiere die neue Nachricht
    new_msg[size + 1] = '\0';

    // Drucke die neue Nachricht (kann modifiziert werden, um sie woanders zu speichern)
    printf("%s", new_msg);

    return 0; 
}




// Select the Baudrate for the UART
#define BAUDRATE 9600

#define BOARD_SIZE 10

#define ROWS 10
#define COLS 10

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
    // Aktivieren des GPIOA- und ADC-Taktes
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN; 
    RCC->APB2ENR |= RCC_APB2ENR_ADCEN; 

    // Setzen des GPIOA-Pins 0 auf den Analogmodus
    GPIOA->MODER |= GPIO_MODER_MODER0;  

    // Setzen des ADC auf den kontinuierlichen Modus und Auswahl der Scan-Richtung
    ADC1->CHSELR |= ADC_CHSELR_CHSEL0 ; 
    ADC1->CFGR1  |= ADC_CFGR1_CONT | ADC_CFGR1_SCANDIR;
   // Einstellen der Abtastzeit
    ADC1->SMPR   |= ADC_SMPR_SMP_0;

    // Wenn der ADC nicht bereit ist, das ADC-Bereitschaftsbit setzen
    if ((ADC1->ISR & ADC_ISR_ADRDY) != 0){   
        ADC1->ISR |= ADC_ISR_ADRDY; 
    }
    // ADC aktivieren
    ADC1->CR |= ADC_CR_ADEN; 
     // Warten, bis der ADC bereit ist
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0){
        if (timeout(TIMEOUT)){
            error();
        }
    }

    // Start the ADC
    ADC1->CR |= ADC_CR_ADSTART;
}

uint16_t ADC_Read(void) {
    // Warten, bis die End-of-Conversion (EOC)-Flagge gesetzt ist
    while ((ADC1->ISR & ADC_ISR_EOC) == 0) {
        if (timeout(TIMEOUT)){
            error();
        }
    }
    // Den Konvertierungswert aus dem Datenregister zurückgeben
    return ADC1->DR;
}


// Funktion zur zufälligen Generierung eines Spielfelds mit festgelegten Schiffen
void generate_field(int field[10][10]) {
    // Initialisiere das Spielfeld mit 0 (kein Schiff)
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            field[i][j] = 0;
        }
    }

    // Definiere die Schiffgrößen
    int ship_sizes[] = {5, 4, 4, 3, 3, 3, 2, 2, 2, 2};
    int num_ships = sizeof(ship_sizes) / sizeof(ship_sizes[0]);

    // Platziere jedes Schiff auf dem Spielfeld
    for (int s = 0; s < num_ships; ++s) {
        int size = ship_sizes[s];
        int placed = 0;

        // Versuche, das Schiff zu platzieren, bis eine gültige Position gefunden wird
        while (!placed) {
            int row = ADC_Read() % 10;
            int col = ADC_Read() % 10;
            int horizontal = ADC_Read() % 2; // 0 für horizontal, 1 für vertikal
            int can_place = 1;

            // Überprüfe die Gültigkeit der Platzierung
            if (horizontal) {
                if (col + size <= 10) {
                    for (int i = col; i < col + size; ++i) {
                        if (field[row][i] != 0) {
                            can_place = 0;
                            break;
                        }
                    }
                } else {
                    can_place = 0;
                }
            } else {
                if (row + size <= 10) {
                    for (int i = row; i < row + size; ++i) {
                        if (field[i][col] != 0) {
                            can_place = 0;
                            break;
                        }
                    }
                } else {
                    can_place = 0;
                }
            }

            // Falls gültig, platziere das Schiff
            if (can_place) {
                if (horizontal) {
                    for (int i = col; i < col + size; ++i) {
                        field[row][i] = size;
                    }
                } else {
                    for (int i = row; i < row + size; ++i) {
                        field[i][col] = size;
                    }
                }
                placed = 1;
            }
        }
    }
}

// Funktion zur Berechnung der Spielfeldchecksumme und Erstellung der Nachricht
void calculate_checksum(int field[10][10], char checksum[]) {
    // Initialisierung der Prüfsumme-Nachricht mit 'CS'
    checksum[0] = 'C';
    checksum[1] = 'S';

    // Zähler für die Position im checksum_msg
    int msg_pos = 2;

    // Zähle die Schiffe in jeder Spalte und füge sie der Nachricht hinzu
    for (int col = 0; col < 10; ++col) {
        int ships_count = 0;
        for (int row = 0; row < 10; ++row) {
            ships_count += (field[row][col] > 0); // Zähle die Schiffe in der aktuellen Spalte
        }
        // Füge die Anzahl der Schiffe der Nachricht hinzu
        checksum[msg_pos++] = (char)(ships_count + '0');
    }

    // Füge den Zeilenumbruch hinzu
    checksum[msg_pos] = '\n';
    checksum[msg_pos + 1] = '\0'; // Nullterminierung der Zeichenkette
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

bool isShotAlreadyTaken(int shot, int takenShots[], int meine_shots) {
    // Durchlaufe die Liste der bereits abgegebenen Schüsse
    for (int i = 0; i < meine_shots; ++i) {
        // Wenn der aktuelle Schuss mit einem der bereits abgegebenen Schüsse übereinstimmt
        if (takenShots[i] == shot) {
            return true; // Gib true zurück, da der Schuss bereits abgegeben wurde
        }
    }
    return false; // Gib false zurück, da der Schuss noch nicht abgegeben wurde
}




// Funktion zur Überprüfung, ob eine Spalte nur noch drei, zwei oder einen freien Platz hat
int check_column_free_spaces(int field[ROWS][COLS], int c) {
    int free_spaces = 0;
    for (int i = 0; i < ROWS; i++) {
        if (field[i][c] == 0) {
            free_spaces++;
        }
    }
    return free_spaces;
}


// Funktion zur Hinzufügung einer kurzen Verzögerung
void delay(uint32_t milliseconds) {
    // Angenommen, eine Taktfrequenz von 48 MHz
    for (uint32_t i = 0; i < milliseconds * 48000; ++i) {
        __NOP();
    }
}

// Für LED
void GPIO_init(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    GPIOA->MODER |= (GPIO_MODER_MODER5_0 | GPIO_MODER_MODER8_0 | GPIO_MODER_MODER9_0 | GPIO_MODER_MODER10_0);
    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT_5 | GPIO_OTYPER_OT_8 | GPIO_OTYPER_OT_9 | GPIO_OTYPER_OT_10);
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR5 | GPIO_OSPEEDR_OSPEEDR8 | GPIO_OSPEEDR_OSPEEDR9 | GPIO_OSPEEDR_OSPEEDR10);
}




int main(void){
    // Configure the system clock to 48MHz
    EPL_SystemClock_Config();

    ADC_Init();
    GPIO_init();

     GPIOA->BSRR = GPIO_BSRR_BR_5; // Set Pin A5 (HIGH)

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
    int field[ROWS][COLS] = {0};
    int ships[3][10] = {
        {3, 3, 3, 2, 2, 3, 4, 4, 4, 4},
        {3, 3, 3, 2, 2, 3, 4, 4, 4, 4},
        {5, 5, 5, 5, 5, 3, 2, 2, 2, 2}
    };

    /*int original_field[ROWS][COLS] = {
            {3, 3, 3, 0, 0, 0, 4, 4, 4, 4},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {3, 3, 3, 0, 0, 0, 4, 4, 4, 4},
            {0, 0, 0, 0, 2, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 2, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 3, 0, 0, 0, 0},
            {0, 0, 0, 2, 0, 3, 0, 0, 0, 0},
            {0, 0, 0, 2, 0, 3, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 2, 2, 0, 0},
            {5, 5, 5, 5, 5, 0, 0, 0, 2, 2}
        };*/
 
    int message_length = 0; // Länge der bisher empfangenen Nachricht
    int message_l_checksum = 0;
    int message_l = 0;
    int message_length_s = 0;

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


    int takenShots[100] = {0};


    for(;;){

        switch(GameState) {
            case WAITING_FOR_START:
                // Überprüfe die Starttaste
                if ((GPIOC->IDR & GPIO_IDR_13) == 0) {
                    // Sende Startnachricht und ändere den Spielzustand
                    LOG("button pressed\n");
                    GAME_LOG("START11928041\n");
                    
                    spieler = 1;
                    GameState = WAITING_FOR_CHECKSUM;
                    break;
                } 
                // Überprüfe, ob Daten über USART2 empfangen wurden
                if (USART2->ISR & USART_ISR_RXNE) {
                    char received_char = USART2->RDR;
                    start[message_length] = received_char;
                    message_length++;
                                
                    // Überprüfe, ob das zuletzt empfangene Zeichen '\n' ist
                    if (received_char == '\n') {
                        LOG("Received: %s", start);
                        spieler = 2; // Spieler auf 2 setzen
                        GameState = GENERATING_FIELD; // Spielzustand entsprechend setzen
                        message_length = 0;
                        break;
                    }
                }
                break; 

            case WAITING_FOR_CHECKSUM:
                // Überprüfe eingehende Nachrichten
                if (USART2->ISR & USART_ISR_RXNE) {
                    char received_c = USART2->RDR;
                    checksum_g[message_l_checksum] = received_c;
                    message_l_checksum++;
                    // Überprüfe auf die Prüfsummen-Nachricht
                    if (received_c == '\n') {
                        checksum_g[message_l_checksum] = '\0'; // Nullterminator hinzufügen
                        LOG("Received checksum: %s", checksum_g);
                        message_l_checksum = 0; // Zähler zurücksetzen
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
                //generate_field(field);
                

                // Calculate checksum
                //calculate_checksum(field, checksum); // zufälliges Spielfeld
                
                //LOG("Checksum calculated: %s", checksum);
                
                
                // Send checksum
                //GAME_LOG("%s", checksum); //Für zufälliges spielfeld
                GAME_LOG("CS3333333333\n");

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
                // Überprüfe eingehende Nachrichten
                if (USART2->ISR & USART_ISR_RXNE) {
                    char received_ch = USART2->RDR;
                    start_1[message_length] = received_ch;
                    message_length++;
                    // Überprüfe, ob das letzte empfangene Zeichen '\n' ist
                    if (received_ch == '\n') {
                        LOG("Received: %s", start_1);
                        kein_treffer = true;
                        schiessen = true;
                        message_length = 0;
                        GameState = PLAYING; // Spielzustand entsprechend setzen
                        break;  
                    }
                }
                break;

            case PLAYING:
                // Game logic

                // Extrahiere Trefferzahlen aus der Prüfsumme
                extract_hit_counts(checksum_g, hit_counts);
               

                // Sortiere Spalten basierend auf den Trefferzahlen
                sort_columns_by_hits(hit_counts, sorted_columns);
                
                

                while (meine_treffer < 30 && treffer_g < 30 && meine_shots <= 100 && shots_g <= 100) {
                    if (schiessen) {
                        if (treffer) {
                            // Benachbartes Feld finden
                            int shot_found = 0;
                            for (int s = 0; s < 4 && !shot_found; ++s) {
                                int new_row = row, new_col = col;
                                switch (s) {
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
                            // Suche zuerst Spalte oder Spalte für Spalte von den sortierten Spalten nach einer zufälligen Zeile
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
                        // Wenn ein Schuss gesendet wurde, überprüfe auf eingehende Nachrichten
                        if (USART2->ISR & USART_ISR_RXNE) {
                            char received_m = USART2->RDR;
                            nachricht[message_l] = received_m;
                            message_l++;

                             // Überprüfe, ob das letzte empfangene Zeichen '\n' ist
                            if (received_m == '\n') {
                                nachricht[message_l] = '\0'; // Nullterminierung des Strings
                                char first_char = nachricht[0];
                                char second_char = nachricht[1];

                                // Überprüfe, ob das erste Zeichen 'T' oder 'W' ist
                                if (first_char == 'T') {
                                    meine_treffer++;
                                    
                                    treffer = true; // Treffer vorhanden
                                } else if (first_char == 'W') {
                                    
                                    kein_treffer = true; // Kein Treffer
                                } else if (first_char == 'S' && second_char == 'F') {
                                    GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BS_9 | GPIO_BSRR_BR_10; // LED leuchtet grün
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
                        // Wenn wir beschossen werden, überprüfe auf eingehende Nachrichten
                        if (USART2->ISR & USART_ISR_RXNE) {
                            char received = USART2->RDR;

                            schuss_g[message_length_s] = received;
                            message_length_s++;

                            // Überprüfe, ob das letzte empfangene Zeichen '\n' ist
                            if (received == '\n') {
                                schuss_g[message_length_s] = '\0';  // Nullterminierung des Strings

                                // Extrahiere Zeilen- und Spalteninformationen aus schuss_g
                                int r = schuss_g[5] - '0';
                                int c = schuss_g[4] - '0';
                                char first_c = schuss_g[0];
                                char second_c = schuss_g[1];

                                if(treffer){
                                    GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BS_9 | GPIO_BSRR_BR_10; // LED leuchtet grün
                                    delay(50);
                                    LOG("We hit at: %d\n", shot);
                                }
                                if(kein_treffer){
                                    GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BR_9 | GPIO_BSRR_BS_10; // LED leuchtet rot
                                    delay(50);
                                    LOG("We miss at: %d\n", shot);
                                }

                                if (first_c == 'S' && second_c == 'F') {
                                    GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BS_9 | GPIO_BSRR_BR_10; // LED leuchtet grün
                                    LOG("SF message received, We won!\n");
                                    GameState = SEND_SF_MESSAGE;
                                    break;
                                } else {

                                     /*if (field[r][c] > 0) {
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
                                        
                                    } Für zufälliges Feld*/


                                    // Kontrollieren wie viele zeilen in der spalte noch frei sind, wenn nur noch 3 sind setze treffer
                                     int free_spaces = check_column_free_spaces(field, c);

                                        if(free_spaces > 3){
                                            field[r][c] = 9;
                                            LOG("Miss received at %d, %d\n", c, r);
                                            GAME_LOG("W\n");

                                        }
                                        else if (free_spaces == 3) {
                                                field[r][c] = ships[0][c];
                                                treffer_g++;
                                                if (treffer_g == 30) {
                                                        GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BR_9 | GPIO_BSRR_BS_10; // LED leuchtet rot
                                                        LOG("30 hits received, We lost!\n");
                                                        GameState = SEND_SF_MESSAGE;
                                                        break;
                                                    }
                                                LOG("Hit received at %d,%d\n", c, r);
                                                GAME_LOG("T\n");
                                                
                                            
                                        } else if (free_spaces == 2) {
                                                field[r][c] = ships[1][c];
                                                treffer_g++;
                                                if (treffer_g == 30) {
                                                        GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BR_9 | GPIO_BSRR_BS_10; // LRD leuchtet rot
                                                        LOG("30 hits received, We lost!\n");
                                                        GameState = SEND_SF_MESSAGE;
                                                        break;
                                                    }
                                                LOG("Hit received at %d,%d\n", c, r);
                                                GAME_LOG("T\n");
                                                
                                            
                                        } else if (free_spaces == 1) {
                                           
                                                field[r][c] = ships[2][c];
                                                treffer_g++;
                                                if (treffer_g == 30) {
                                                        GPIOA->BSRR = GPIO_BSRR_BS_8 | GPIO_BSRR_BR_9 | GPIO_BSRR_BS_10; // LED leuchtet rot
                                                        LOG("30 hits received, We lost!\n");
                                                        GameState = SEND_SF_MESSAGE;
                                                        break;
                                                    }
                                                LOG("Hit received at %d,%d\n", c, r);
                                                GAME_LOG("T\n");
                                                
                                            }
                                        
                                    shots_g++;

                                }

                                message_length_s = 0; 
                                memset(schuss_g, 0, sizeof(schuss_g));
                                schiessen = true;
                                beschossen_werden = false;
                            }
                        }
                    }
                }
                break;

            case SEND_SF_MESSAGE:


                // Für schummeln
                 for (int i = 0; i < 10; ++i){
                    for (int j = 0; j < 10; ++j){
                        if(field[j][i] == 9){
                            field[j][i] = 0;
                        }
                    }
                }
                // Kontrollieren, ob jede spalte drei treffer hat, sonst setze treffer für SF-message
                for (int c = 0; c < COLS; ++c) {
                    int hit_count = 0;
                    for (int r = 0; r < ROWS; ++r) {
                        if (field[r][c] != 0) {
                            hit_count++;
                        }
                    }
                    for (int r = 0; r < ROWS && hit_count < 3; ++r) {
                        if(hit_count == 0){
                        if (field[r][c] == 0) {
                            field[r][c] = ships[0][c];  
                            hit_count++;
                        }
                        }
                        if(hit_count == 1){
                        if (field[r][c] == 0) {
                            field[r][c] = ships[1][c];  
                            hit_count++;
                        }
                        }
                        if(hit_count == 2){
                        if (field[r][c] == 0) {
                            field[r][c] = ships[2][c];  
                            hit_count++;
                        }
                        }

                    }
                }


               


                for (int col = 0; col < COLS; ++col) {
                    // Construct the SF message for each column
                    char sf_message[16];
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