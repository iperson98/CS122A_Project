/*
 * HomeSecuritySystemV4.c
 *
 * Created: 11/14/2019 
 * Author : Alex Madera
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "io.c"
#include "keypad.h"

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;	// Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}
void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}
void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

//========================================================================
// BEGIN: Exercise Code
//========================================================================

void transmit_data(unsigned char data, int choose){
	int i;
	if(choose){
		for (i = 0; i < 8 ; ++i) {
			// Sets SRCLR to 1 allowing data to be set
			// Also clears SRCLK in preparation of sending data
			PORTD = 0x08;
			// set SER = next bit of data to be sent.
			PORTD |= ((data >> i) & 0x01);
			// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
			PORTD |= 0x02;
		}
		// set RCLK = 1. Rising edge copies data from ?Shift? register to ?Storage? register
		PORTD |= 0x04;
		// clears all lines in preparation of a new transmission
		PORTD = 0x00;
	}else
	{
		for (i = 0; i < 8 ; ++i) {
			// Sets SRCLR to 1 allowing data to be set
			// Also clears SRCLK in preparation of sending data
			PORTA = 0x08;
			// set SER = next bit of data to be sent.
			PORTA |= ((data >> i) & 0x01);
			// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
			PORTA |= 0x02;
		}
		// set RCLK = 1. Rising edge copies data from ?Shift? register to ?Storage? register
		PORTA |= 0x04;
		// clears all lines in preparation of a new transmission
		PORTA = 0x00;
	}
	
}

typedef struct Tasks {
	//Task's current state, period, and the time elapsed
	// since the last tick
	signed char state;
	unsigned long int period;
	unsigned long int elapsedTime;
	//Task tick function
	int (*TickFct)(int);
} task;

enum LED_States {led_display} led_state;
enum LCD_States {init_lcd, lcd_display, lcd_wait, lcd_password, lcd_arm, lcd_armed, lcd_intrustion, lcd_unarm, lcd_unarmed} lcd_state;
enum Keypad_States {keypad_enter, keypad_password, keypad_compare, keypad_wait, keypad_compare_unarm, keypad_wait_again} keypad_state;
enum Button_States {button_init, button_idle, button_hold, button_release} button_state;
	
unsigned char alarm_switch = 0;
int system_intrusion = 0;
unsigned char lcd_counter = 0;
unsigned char password_counter = 0;
unsigned char currentKey = 0x00;
unsigned char setKey = 0x00;
char password[5];
char arm_password[5];
unsigned char buttonKey = 0x00;

void LEDFct() {
	// === Local Variables ===
	static unsigned char column_val = 0x01; // sets the pattern displayed on columns
	static unsigned char column_sel = 0x00; // grounds column to display pattern
	switch(led_state) {
		case led_display:
		led_state = led_display;
		break;
	}
	
	// State Actions
	switch(led_state) {
		case led_display:
		if(alarm_switch && system_intrusion) {
		column_sel = 0x00;
		column_val = 0xFF;
		}
		else {
		column_sel = 0x00;
		column_val = 0x00;	
		}
		alarm_switch = !alarm_switch;
		break;
	}
	transmit_data(column_sel, 1);
	transmit_data(column_val, 0);
}

void LCDFct() {
	// === Local Variables ===

	switch(lcd_state) {
		case init_lcd:
		lcd_state = lcd_display;
		break;
		case lcd_display:
		lcd_state = lcd_wait;
		break;
		case lcd_wait:
		lcd_state = lcd_wait;
		if(keypad_state == keypad_password) {
			lcd_state = lcd_password;
		}
		break;
		case lcd_password:
		lcd_state = lcd_password;
		if(keypad_state == keypad_compare) {
			lcd_state = lcd_arm;
			lcd_counter = 0;
		}
		break;
		case lcd_arm:
		lcd_state = lcd_arm;
		if(keypad_state == keypad_wait) {
			lcd_state = lcd_armed;
			lcd_counter = 0;
		}
		break;
		case lcd_armed:
		lcd_state = lcd_armed;
		if(system_intrusion) {
			lcd_state = lcd_intrustion;
		}
		break;
		case lcd_intrustion:
		lcd_state = lcd_intrustion;
		lcd_counter++;
		if(lcd_counter > 2) {
			lcd_state = lcd_unarm;
			lcd_counter = 0;
		}
		break;
		case lcd_unarm:
		lcd_state = lcd_unarm;
		if(keypad_state == keypad_wait_again) {
			lcd_state = lcd_unarmed;
			lcd_counter = 0;
		}
		break;
		case lcd_unarmed:
		lcd_state = lcd_unarmed;
		system_intrusion = 0;
		lcd_counter++;
		if(lcd_counter > 3) {
			lcd_state = lcd_arm;
			lcd_counter = 0;
			keypad_state = keypad_compare;
			arm_password[0] = 0x00;
			arm_password[1] = 0x00;
			arm_password[2] = 0x00;
			arm_password[3] = 0x00;
			arm_password[4] = 0x00;
		}
		break;
	}
	
	// State Actions
	switch(lcd_state) {
		case init_lcd:
		case lcd_display:
		LCD_DisplayString(1, "Welcome to Home Security!");
		break;
		case lcd_wait:
		LCD_DisplayString(1, "To Set Password Press #");
		break;
		case lcd_password:
		if(!(lcd_counter > 1)) {
			LCD_DisplayString(1, "Enter 4 Digit Password + <#>");
			lcd_counter++;
		}
		else {
			LCD_DisplayString(1, "Password: ");
			LCD_Cursor(11);
			if(password[0] > 0x09) {
				LCD_WriteData(password[0]);
			}
			else {
				LCD_WriteData(password[0] + '0');
			}
			LCD_Cursor(12);
			if(password[1] > 0x09) {
				LCD_WriteData(password[1]);
			}
			else {
				LCD_WriteData(password[1] + '0');
			}
			LCD_Cursor(13);
			if(password[2] > 0x09) {
				LCD_WriteData(password[2]);
			}
			else {
				LCD_WriteData(password[2] + '0');
			}
			LCD_Cursor(14);
			if(password[3] > 0x09) {
				LCD_WriteData(password[3]);
			}
			else {
				LCD_WriteData(password[3] + '0');
			}
		}
		break;
		case lcd_arm:
		if(!(lcd_counter > 3)) {
			LCD_DisplayString(1, "Enter <Password> + <#> to Arm");
			lcd_counter++;
			arm_password[4] = 0;
		}
		else {
			LCD_DisplayString(1, "Password: ");
			LCD_Cursor(11);
			if(arm_password[0] > 0x09) {
				LCD_WriteData(arm_password[0]);
			}
			else {
				LCD_WriteData(arm_password[0] + '0');
			}
			LCD_Cursor(12);
			if(arm_password[1] > 0x09) {
				LCD_WriteData(arm_password[1]);
			}
			else {
				LCD_WriteData(arm_password[1] + '0');
			}
			LCD_Cursor(13);
			if(arm_password[2] > 0x09) {
				LCD_WriteData(arm_password[2]);
			}
			else {
				LCD_WriteData(arm_password[2] + '0');
			}
			LCD_Cursor(14);
			if(arm_password[3] > 0x09) {
				LCD_WriteData(arm_password[3]);
			}
			else {
				LCD_WriteData(arm_password[3] + '0');
			}
		}
		break;
		case lcd_armed:
		LCD_DisplayString(1, "System Armed -- On");
		break;
		case lcd_intrustion:
		LCD_DisplayString(1, "System Intrusion");
		break;
		case lcd_unarm:
		if(!(lcd_counter > 1)) {
			LCD_DisplayString(1, "Enter <Password> + <#> to Un-arm");
			lcd_counter++;
			arm_password[4] = 0;
		}
		else {
			LCD_DisplayString(1, "Password: ");
			LCD_Cursor(11);
			if(arm_password[0] > 0x09) {
				LCD_WriteData(arm_password[0]);
			}
			else {
				LCD_WriteData(arm_password[0] + '0');
			}
			LCD_Cursor(12);
			if(arm_password[1] > 0x09) {
				LCD_WriteData(arm_password[1]);
			}
			else {
				LCD_WriteData(arm_password[1] + '0');
			}
			LCD_Cursor(13);
			if(arm_password[2] > 0x09) {
				LCD_WriteData(arm_password[2]);
			}
			else {
				LCD_WriteData(arm_password[2] + '0');
			}
			LCD_Cursor(14);
			if(arm_password[3] > 0x09) {
				LCD_WriteData(arm_password[3]);
			}
			else {
				LCD_WriteData(arm_password[3] + '0');
			}
		}
		break;
		case lcd_unarmed:
		LCD_DisplayString(1, "System Unarmed -- Off");
		break;
	}
}

void KeypadFct() {
	// === Local Variables ===

	switch(keypad_state) {
		case keypad_enter:
		keypad_state = keypad_enter;
		currentKey = GetKeypadKey();
		if(currentKey == 0x23) {
			keypad_state = keypad_password;
		}
		break;
		case keypad_password:
		if(lcd_state == lcd_password && lcd_counter > 1) {
		keypad_state = keypad_password;
		setKey = GetKeypadKey();
		if(setKey != 0x00) {
		password[password_counter] = setKey;
		password_counter++;
		}
		setKey = 0x00;
		if(password[4] == 0x23) {
			keypad_state = keypad_compare;
			password_counter = 0;
		}
		else if(password[4] != 0x00) {
			password[0] = 0x00;
			password[1] = 0x00;
			password[2] = 0x00;
			password[3] = 0x00;
			password[4] = 0x00;
			password_counter = 0;
			lcd_state = lcd_password;
			lcd_counter = 0;
		}
		}
		else {
			keypad_state = keypad_password;
		}
		break;
		case keypad_compare:
		if(lcd_counter > 1) {
		keypad_state = keypad_compare;
		setKey = GetKeypadKey();
		if(setKey != 0x00) {
			arm_password[password_counter] = setKey;
			password_counter++;
		}
		setKey = 0x00;
		if(arm_password[0] == password[0] &&
		arm_password[1] == password[1] &&
		arm_password[2] == password[2] &&
		arm_password[3] == password[3] &&
		arm_password[4] == password[4]) {
			keypad_state = keypad_wait;
			password_counter = 0;
		}
		if(arm_password[0] != 0x00 &&
		arm_password[1] != 0x00 &&
		arm_password[2] != 0x00 &&
		arm_password[3] != 0x00 &&
		arm_password[4] != 0x00 &&
		arm_password[4] != 0x23) {
			password_counter = 0;
			lcd_counter = 0;
			lcd_state = lcd_arm;
			keypad_state = keypad_compare;
			arm_password[0] = 0x00;
			arm_password[1] = 0x00;
			arm_password[2] = 0x00;
			arm_password[3] = 0x00;
			arm_password[4] = 0x00;
		}
		if((arm_password[0] != password[0] ||
		arm_password[1] != password[1] ||
		arm_password[2] != password[2] ||
		arm_password[3] != password[3]) &&
		arm_password[4] == 0x23) {
			password_counter = 0;
			lcd_counter = 0;
			lcd_state = lcd_arm;
			keypad_state = keypad_compare;
			arm_password[0] = 0x00;
			arm_password[1] = 0x00;
			arm_password[2] = 0x00;
			arm_password[3] = 0x00;
			arm_password[4] = 0x00;
		}
		}
		else {
			keypad_state = keypad_compare;
		}
		break;
		case keypad_wait:
		keypad_state = keypad_wait;
		if(lcd_state == lcd_unarm) {
			keypad_state = keypad_compare_unarm; 
			password_counter = 0;
			arm_password[0] = 0x00;
			arm_password[1] = 0x00;
			arm_password[2] = 0x00;
			arm_password[3] = 0x00;
			arm_password[4] = 0x00;
		}	
		break;
		case keypad_compare_unarm:
				if(lcd_counter > 1) {
					keypad_state = keypad_compare_unarm;
					setKey = GetKeypadKey();
					if(setKey != 0x00) {
						arm_password[password_counter] = setKey;
						password_counter++;
					}
					setKey = 0x00;
					if(arm_password[0] == password[0] &&
					arm_password[1] == password[1] &&
					arm_password[2] == password[2] &&
					arm_password[3] == password[3] &&
					arm_password[4] == password[4]) {
						keypad_state = keypad_wait_again;
						password_counter = 0;
					}
					if(arm_password[0] != 0x00 &&
					arm_password[1] != 0x00 &&
					arm_password[2] != 0x00 &&
					arm_password[3] != 0x00 &&
					arm_password[4] != 0x00 &&
					arm_password[4] != 0x23) {
						password_counter = 0;
						lcd_counter = 0;
						lcd_state = lcd_unarm;
						keypad_state = keypad_compare_unarm;
						arm_password[0] = 0x00;
						arm_password[1] = 0x00;
						arm_password[2] = 0x00;
						arm_password[3] = 0x00;
						arm_password[4] = 0x00;
					}
					if((arm_password[0] != password[0] ||
					arm_password[1] != password[1] ||
					arm_password[2] != password[2] ||
					arm_password[3] != password[3]) &&
					arm_password[4] == 0x23) {
						password_counter = 0;
						lcd_counter = 0;
						lcd_state = lcd_unarm;
						keypad_state = keypad_compare_unarm;
						arm_password[0] = 0x00;
						arm_password[1] = 0x00;
						arm_password[2] = 0x00;
						arm_password[3] = 0x00;
						arm_password[4] = 0x00;
					}
				}
				else {
					keypad_state = keypad_compare_unarm;
				}
		break;
		case keypad_wait_again:
		keypad_state = keypad_wait_again;
		break;
	}
	
	// State Actions
	switch(keypad_state) {
		case keypad_enter:
		break;
		case keypad_password:
		break;
		case keypad_compare:
		break;
		case keypad_wait:
		break;
		case keypad_compare_unarm:
		break;
		case keypad_wait_again:
		break;
	}
}

void ButtonFct() {
	// === Local Variables ===

	switch(button_state) {
		case button_init:
		button_state = button_idle;
		break;
		case button_idle:
		button_state = button_idle;
		buttonKey = GetKeypadKey();
		if(keypad_state == keypad_wait && buttonKey == 0x2A) {
			button_state = button_release;
		}
		break;
		case button_release:
		button_state = button_idle;
		break;
	}
	
	// State Actions
	switch(button_state) {
		case button_init:
		break;
		case button_idle:
		break;
		case button_release:
		system_intrusion = 1;
		break;
	}
}

int main(void)
{
	DDRA = 0xFF; PORTA= 0x00;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xF0; PORTC = 0x0F;
	DDRD = 0xFF; PORTD = 0x00;

	
	TimerSet(1);
	TimerOn();
	LCD_init();

	static task task1, task2, task3, task4;
	task *tasks[] = {&task1, &task2, &task3, &task4};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	unsigned short i; // Scheduler for-loop iterator
	
	// Periods for the tasks
	unsigned long int LEDFct_Period = 500;
	unsigned long int LCDFct_Period = 1000;
	unsigned long int KeypadFct_Period = 500;
	unsigned long int ButtonFct_Period = 100;
	
	// Task 1
	task1.state = led_display;//Task initial state.
	task1.period = LEDFct_Period;//Task Period.
	task1.elapsedTime = LEDFct_Period;//Task current elapsed time.
	task1.TickFct = &LEDFct;//Function pointer for the tick.
	
	// Task 2
	task2.state = init_lcd;//Task initial state.
	task2.period = LCDFct_Period;//Task Period.
	task2.elapsedTime = LCDFct_Period;//Task current elapsed time.
	task2.TickFct = &LCDFct;//Function pointer for the tick.
	
	// Task 3
	task3.state = keypad_enter;//Task initial state.
	task3.period = KeypadFct_Period;//Task Period.
	task3.elapsedTime = KeypadFct_Period;//Task current elapsed time.
	task3.TickFct = &KeypadFct;//Function pointer for the tick.
	
	// Task 4
	task4.state = button_init;//Task initial state.
	task4.period = ButtonFct_Period;//Task Period.
	task4.elapsedTime = ButtonFct_Period;//Task current elapsed time.
	task4.TickFct = &ButtonFct;//Function pointer for the tick.
	
	while(1) {
		// Scheduler code
		for ( i = 0; i < numTasks; i++ )
		{
			// Task is ready to tick
			if ( tasks[i]->elapsedTime == tasks[i]->period )
			{
				// Setting next state for task
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				// Reset the elapsed time for next tick.
				tasks[i]->elapsedTime = 0;
			}
			
			tasks[i]->elapsedTime += 1;
		}
		while (!TimerFlag);	// Wait .1 sec
		TimerFlag = 0;
	}
	//========================================================================
	// END: Exercise Code
	//========================================================================
}