#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BUTTON_PIN PD4
#define DOT_TIME 100
#define DASH_TIME 300
#define SEND_TIME 500
#define CLOSE_TIME 10000

#define PCF_ADDR 0x27
#define LCD_BL 0x08
#define TWI_SCL_HZ 100000UL
#define TWBR_VAL ((uint8_t)((F_CPU / TWI_SCL_HZ - 16UL) / 2UL))

#define TWI_START()  do {                              \
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);           \
    while (!(TWCR & (1<<TWINT)));                      \
} while (0)

#define TWI_STOP()   do {                              \
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);           \
    while (TWCR & (1<<TWSTO));                         \
} while (0)

static char letters[] = {'_', 'E', 'T', 'I', 'A', 'N', 'M', 'S', 'U', 'R', 'W', 'D', 'K', 'G', 'O', 'H', 'V', 'F', '_', 'L', '_', 'P', 'J', 'B', 'X', 'C', 'Y', 'Z', 'Q', '_', '_', '5', '4', '_', '3', '_', '_', '_', '2', '_', '_', '+', '_', '_', '_', '_', '1', '6', '=', '/', '_', '_', '_', '_', '_', '7', '_', '_', '_', '8', '_', '9', '0'};
volatile char letter = 0;
volatile uint8_t lcd_flag = 0;
volatile uint8_t clear_flag = 0;

static void twi_write_byte(uint8_t b)
{
    TWDR = b;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

static void twi_init(void)
{
    TWBR = TWBR_VAL;
    TWSR = 0x00;        
}

static void pcf_write(uint8_t data)
{
    TWI_START();
    twi_write_byte((uint8_t)(PCF_ADDR << 1 | 0));
    twi_write_byte(data);
    TWI_STOP();
}

static void init_timer()
{
    TCCR0A |= (1 << WGM01);
    TCCR0B |= (1 << CS01) | (1 << CS00);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
}

static void lcd_init(void)
{
    twi_init();
    _delay_ms(50);                  /* power-on wait ≥ 40 ms               */

    /* Three 0x30 nibbles — reset to 8-bit mode, then switch to 4-bit */
    lcd_write_nibble(0x30, 0); _delay_ms(5);
    lcd_write_nibble(0x30, 0); _delay_us(150);
    lcd_write_nibble(0x30, 0); _delay_us(150);
    lcd_write_nibble(0x20, 0); _delay_us(150);  /* switch to 4-bit mode    */

    lcd_command(0x28);   /* Function Set — 4-bit, 2 lines, 5×8             */
    lcd_command(0x0C);   /* Display ON, cursor OFF, blink OFF               */
    lcd_command(0x06);   /* Entry mode — cursor right, no shift             */
    lcd_command(0x01);   /* Clear display (needs ≥ 1.52 ms wait)           */
    _delay_ms(2);
}

static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t base = (nibble & 0xF0) | LCD_BL | (rs ? 0x01 : 0x00);
    pcf_write((uint8_t)(base | 0x04));  
    _delay_us(1);                         
    pcf_write((uint8_t)(base & ~0x04));  
    _delay_us(50);                        
}

static void lcd_send(uint8_t byte, uint8_t rs)
{
    lcd_write_nibble(byte & 0xF0, rs);
    lcd_write_nibble((uint8_t)(byte << 4), rs);
}

static void lcd_command(uint8_t cmd)
{
    lcd_send(cmd, 0);
    if (cmd <= 0x03)
        _delay_ms(2);   
}

static void lcd_char(char c){ lcd_send((uint8_t)c, 1); }

ISR(TIMER0_COMPA_vect)
{
    static uint32_t push_ms = 0;
    static uint32_t close_ms = 0;
    static uint32_t send_ms = 0;
    static uint8_t prev_button_state = 1;
    static uint8_t i = 0;

    uint8_t button_state = (PIND & (1 << BUTTON_PIN)) ? 1 : 0;
    
    // tuş basılı
    if(button_state == 0)
    {
        push_ms++;
        close_ms = 0;
    }

    // tuştan eli çektik
    else if(button_state == 1 && prev_button_state == 0)
    {
        // dash mi
        if(push_ms >= DASH_TIME)
        {
            i = 2 * i+2;
            if (letters[i] != '_' && i <= 63)
                letter = letters[i];
            else
                i = 0;
        }

        // dot mu
        else if(push_ms >= DOT_TIME)
        {
            i = 2 * i+1;
            if (letters[i] != '_' && i <= 63)
                letter = letters[i];
            else
                i = 0;
        }
        push_ms = 0;
    }

    // hiçbişi yapmıyoz
    else if(button_state == 1 && prev_button_state == 1 && !lcd_flag)
    {
        send_ms++;
        close_ms++;
        if(send_ms >= SEND_TIME && letter != 0)
        {
            lcd_flag = 1;
            send_ms = 0;
            i = 0;
        }
        if(close_ms >= CLOSE_TIME & letter == 0)
        {
            clear_flag = 1;
            close_ms = 0;
        }
    }
    prev_button_state = button_state;
}

int main(void)
{
    // 4. pin input
    DDRD &= ~(1 << BUTTON_PIN);
    PORTD |= (1 << BUTTON_PIN);
    
    // internal led
    DDRB |= (1 << PB5);

    init_timer();
    lcd_init();
    sei();

    while(1)
    {
        if (lcd_flag)
        {
            PORTB |= (1<<PB5);
            _delay_ms(30);
            PORTB &= ~(1<<PB5);
            lcd_char(letter);
            letter = 0;
            lcd_flag = 0;
        }
        if(clear_flag)
        {
          lcd_command(0x01);
          clear_flag = 0;
        }
    }
}
