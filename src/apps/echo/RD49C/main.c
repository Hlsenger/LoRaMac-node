#include <string.h>
#include "board.h"
#include "gpio.h"
#include "delay.h"
#include "timer.h"
#include "radio.h"
#include "uart.h"
#include "board-config.h"

#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             20        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       12         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         5         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 64 // Define the payload size here


uint8_t buffer[256];
static RadioEvents_t RadioEvents;


extern Gpio_t Gpio0;

extern Gpio_t Led1;
extern Gpio_t Led2;
extern Gpio_t Led3;
extern Uart_t Uart2;


typedef enum
{
    STANDBY,
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    TX,
    TX_TIMEOUT,
}States_t;


uint8_t isMaster = 0;
States_t state = STANDBY;


void OnTxDone(){
    Radio.Sleep();
    if(isMaster){
        state = RX;
    }else{
        state = STANDBY;
    }

}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ){
    GpioToggle(&Led3);
    Radio.Sleep();

    if(isMaster){
        UartPutBuffer(&Uart2,payload,size);
        state=STANDBY;
    }else{
        //Echo back
        state=TX;
        Radio.Send(payload,size);
    }
}

void OnTxTimeout(){
     Radio.Sleep();
    //If never recieved an anwser, try again
    state=STANDBY;
}

void OnRxTimeout(){
    Radio.Sleep();
    //If never recieved an anwser, try again
    state=STANDBY;

}

void OnRxError(){
    Radio.Sleep();
    //If recieve error, try again
    state=STANDBY;
}

int main( void )
{
    //Board initialziation
    BoardInitMcu( );
    BoardInitPeriph( );

    // Radio initialization
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );


    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

    Radio.SetMaxPayloadLength( MODEM_LORA, BUFFER_SIZE );

     GpioInit( &Gpio0, GPIO_2, PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0 );

    //Delay before loop to allow the debugger to connect
    DelayMs(1000);

    if(GpioRead(&Gpio0) == 0){
        GpioToggle(&Led1);
        isMaster = 1;
    }else{
        GpioToggle(&Led2);
        isMaster = 0;
    }

   


   


    while(1){
        if(isMaster){
            switch(state){
                case STANDBY:
                state = TX;
                Radio.Send("oi",2);
                break;
                case RX:
                Radio.Rx(RX_TIMEOUT_VALUE);
                break;
                case TX:
                break;

                default:
                break;
            }



            
        }else{
            switch(state){
                case STANDBY:
                state = RX;
                break;

                case RX:
                Radio.Rx(RX_TIMEOUT_VALUE);
                break;

                case TX:
                //Wait for 5 seconds, if the answer is never echoed back, try again

                break;

                default:
                break;
            }
        }



        // Process Radio IRQ
        if( Radio.IrqProcess != NULL )
        {
            Radio.IrqProcess( );
        }
        

        DelayMs(1);
    }



    return 0;
}