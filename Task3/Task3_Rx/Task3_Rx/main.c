/*
 * Task3_Rx.c
 *
 * Created: 10/24/2019 9:53:40 AM
 * Author : Ahmed
 */ 
#define F_CPU (8000000) //Set clock frequency
#define DEL_BAR (6)

#include <avr/io.h>
#include <stdlib.h>
#include "Display/graphics.h"
#include "Display/mylcd.h"
#include "Display/font4x8.h"
#include "Display/font6x8.h"
#include "Display/font5x8.h"
#include "i2c_lib.h"
#include "PWM_lib.h"
#include "timer_lib.h"
#include "UART_lib.h"
#include "EEPROM_lib.h"
#include <stdbool.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>


// Variable for receiving data from i2c
volatile uint8_t ui8_RX = 0;

// Variable for calculation volts
volatile float f_volt=0;

// data buffer for conversion from integer/float value to string
volatile char c_data_array[7];

// time variable for plotting
volatile uint8_t ui8_time = 0;

// variable to store current time point for interpolation
volatile uint8_t ui8_time1 = 0;

// Delete bar to empty the screen so data won't overlap
volatile uint8_t ui8_delbar = DEL_BAR;

// Variables to hold current and data points
volatile uint8_t ui8_scale =0;
volatile uint8_t ui8_scale1 =44;

//i2c slave address
volatile uint8_t ui8_address = 0x21;
volatile uint8_t ui8_eeprom_address = 0x07;


//push button variable and communication variable
uint8_t press=0;
bool protocol=0;      // I2C:1  , UART:0

// Preprocessor for plot function
void Plot(uint8_t ui8_RX);

void push_button();
void switch_communication();
void watchdog_switch();


ISR(INT0_vect){

if(PIND & (1<<PD2))
protocol = 1;

else
protocol=0;

}


ISR(USART_RXC_vect){
   
     wdt_reset();														// Reset watchdog timer

    check_parity();
     ui8_RX = UDR;
     timer0_cycle(ui8_RX);
     Plot(ui8_RX);
   
}

ISR(TIMER1_COMPB_vect){


  if (protocol==1){ 
  // Start i2c transmission
  i2c_start(ui8_address);

  // Red data from I2C
  ui8_RX = i2c_readack();

  // Set PWM output for LED Brightness
  timer0_cycle(ui8_RX);

  //Reset Watchdog timer
  wdt_reset();

  // Plot the data on the LCD
  Plot(ui8_RX);

  //Send stop signal to end transmission
  i2c_stop();
  }
  //Toggle LED for testing
  //PORTB ^= (1<<PB4);


  switch_communication();
  


  }
  


int main(void)
{
  
   if(PIND & (1<<PD2))
   protocol = 1;
   else
   protocol=0;


  //Initialize LCD
  lcd_init();
  lcd_clear();
  lcd_set_cursor(0,0);
  


   //Initialize UART
   uart_init(9600);

   //button press
   push_button();

  //Initialize Timer for PWM output
  timer0_init();

  //initialize time 1 for controlling i2c data transmission
  timer1_init();

  //Initialize UART
  uart_init(9600);
  
  //Initialize I2c
  i2c_master_init();

 

  //Set pins for testing
  DDRB |= (1<<DDB4) | (1<<DDB6);



    //Check watchdog timer status
    if(MCUCSR&(1<<WDRF))
    {
      //If condition is true, display error message for 2 seconds
      lcd_puts(font5x8,"System Failure");
      _delay_ms(2000);

      //Switch communication
      protocol= EEPROM_read(ui8_eeprom_address);
      protocol = !protocol;
      switch_communication();

      //Clear Watchdog reset pin
      WatchDog_clear();

      //Clear LCD from error message
      lcd_clear();
    }

  //Enable WatchDog timer
  WatchDog_on();
  
  //Enable Global Interrupt
  sei();


  
  
  while (1)
  {
  }
}




void Plot(uint8_t ui8_RX){

  // Calculate volts from ADC value
  f_volt = (ui8_RX*5.0)/255;

  // Convert the value into a string for printing
  dtostrf(f_volt,3,2,c_data_array);
  
  // Set cursor location for printing
  lcd_set_cursor(0,55);

  // print the volt value
  lcd_puts(font5x8,c_data_array);
  lcd_puts(font5x8,"v");

  //Showing current communication protocol
  if (protocol ==1)
  lcd_puts(font5x8,"  I2C ");
  else
  lcd_puts(font5x8,"  UART");


  // Set curve scale
  ui8_scale = 44 - (ui8_RX*40)/255;

  // Plot graph using interpolation function
  g_Interp1(ui8_time1,ui8_scale1,ui8_time,ui8_scale,1);
  
  // Create delete bar to update the curve
  g_VLine(ui8_delbar,0,50,0);

  // Save Current data & time points
  ui8_scale1 = ui8_scale;
  ui8_time1 = ui8_time;

  // Increment time-axis and delete bar axis
  ui8_time++;
  ui8_delbar++;


  // if time-axis reaches the end of LCD (128) --> reset time axis
  if(ui8_time>125){
    ui8_time =0;
    ui8_time1=0;
  }

  if(ui8_delbar>125){
    ui8_delbar=0;
  }


}


void push_button(){

  DDRD &= ~(1<<PD2);

  // Enable external interrupt at PD3 ( INT1 )
  GICR |= (1<<INT0);

  // Any logical change will generate an interrupt request
  MCUCR |= (1<<ISC00);

}


void switch_communication(){

  if(protocol==1)
  {
    TWCR |= (1<<TWEN);
    UCSRB &= ~((1<<RXEN)| (1<<RXCIE));
    PORTB |= (1<<PB4);
    EEPROM_write(ui8_eeprom_address,protocol);
  }
  
  
  else if (protocol==0)
  {
    TWCR &= ~(1<<TWEN);
    UCSRB |= (1<<RXEN)| (1<<RXCIE);
    PORTB &=~ (1<<PB4);
    EEPROM_write(ui8_eeprom_address,protocol);
  }

}



void watchdog_switch(){


if(protocol==1)
{
  //TWCR |= (1<<TWEN);
  UCSRB &= ~((1<<RXEN)| (1<<RXCIE));
  PORTB |= (1<<PB4);
}


else if (protocol==0)
{
  TWCR &= ~(1<<TWEN);
  UCSRB |= (1<<RXEN)| (1<<RXCIE);
  PORTB &=~ (1<<PB4);
}



}

