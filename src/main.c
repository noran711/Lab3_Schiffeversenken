#include <stm32f0xx.h>
#include "mci_clock.h"
#include <stdio.h>

#define DEBUG

// This is a simple macro to print debug messages if DEBUG is defined
#ifdef DEBUG
  #define LOG( msg... ) printf( msg );
#else
  #define LOG( msg... ) ;
#endif

// Select the Baudrate for the UART
#define BAUDRATE 9600

// For supporting printf function we override the _write function to redirect the output to UART
int _write( int handle, char* data, int size ) {
    int count = size;
    while( count-- ) {
        while( !( USART2->ISR & USART_ISR_TXE ) ) {};
        USART2->TDR = *data++;
    }
    return size;
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
        SPIELER1,
        SPIELER2,
        GENERATING_FIELD,
        PLAYING,
    };

    enum GameState GameState = WAITING_FOR_START;

    for(;;){
        // Wait for the data to be received
        //while( !( USART2->ISR & USART_ISR_RXNE ) );

        // Read the data from the RX buffer
        //rxb = USART2->RDR;

        // Print the received data to the console
        //LOG("[DEBUG-LOG]: %d\r\n", rxb );
        
        switch (GameState){
            case WAITING_FOR_START:
                //check for incoming messages
                if (USART2->ISR & USART_ISR_RXNE) {
                    rxb = USART2->RDR;
                    // Check for start message
                    if (rxb == 'START'){
                        GameState = SPIELER2;
                        break;
                    } 
                }else if((GPIOC->IDR & GPIO_IDR_13) == 0) {
                            LOG("START11928041\n");
                            delay(100);
                            GameState = SPIELER1;
                }
                break;

            case SPIELER1:
                // Generate the start for player 1


                break;

            case SPIELER2:
                // Generate the start for player 2

                break;


            case PLAYING:
                // Game logic

                break;

            
        }




    
        }
    }
