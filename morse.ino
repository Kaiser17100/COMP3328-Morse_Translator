#include <avr/io.h>
#include <util/delay.h>

#define CPU_SPEED 16000000UL // atmega328p speed
#define UNIT_TIME 10
#define LED_PIN PD4


static void delay_ms(uint32_t ms)
{
  for (uint32_t i = 0; i < ms; i++)
    _delay_ms(1);
}

int main(void)
{
    // pin D13 output
    DDRB |= (1 << LED_PIN);
    // pin D2 input
    DDRD &= ~(1 << PD2);
    PORTB |= (1 << PD2);
    
    while(1)
    {
      if(PIND & (1 << PD2))
      {
        delay_ms(20);
        if(!(PIND & (1 << PD2)))
          PORTB |= (1 << LED_PIN);
      }
      else
        PORTB &= ~(1 << PB5);
    }
    return 0;
}
