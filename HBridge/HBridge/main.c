/*
 * HBridge.c
 *
 * Created: 12/31/2023 2:58:21 PM
 * Author : Nathan Stark
 */ 

#define F_CPU 8000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>

int8_t motor_speed;
int8_t prev_motor_speed;

uint8_t servo_pulse_flag;
uint8_t servo_timeout_flag;
int16_t servo_pwm_time;

ISR(TIMER0_COMPA_vect) {
	// timer has rolled over, so reverse direction of HBridge for a short time to allow capacitor to recharge
	TCCR0B &= ~0x02; // stop timer
	if(motor_speed < 0) {
		PORTD = 0x42; // turn on high 0 and low 1 to run motor backward
	} else if(motor_speed > 0) {
		PORTD = 0x81; // turn on high 1 and low 0 to run motor forward
	} else {
	}
	TCCR0B |= 0x02; // restart timer
}

ISR(TIMER0_COMPB_vect) {
	PORTD = 0x41; // turn the two low FETs on to recharge caps
}

ISR(TIMER1_COMPA_vect) {
	// a timeout has occurred waiting for another pulse
	TCCR1B &= ~0x02;
	TCNT1 = 0x0000;
	servo_timeout_flag = 1;
	EICRA = 0x03; // wait for next rising edge
	
}

ISR(INT0_vect) {
	// stop timer
	TCCR1B &= ~0x02;
	if(PIND & 0x04) {
		// rising edge happened
		EICRA = 0x02;
	} else {
		// falling edge happened
		servo_pwm_time = TCNT1;
		EICRA = 0x03;
		servo_pulse_flag = 1;
	}
	
	// reset time and restart
	TCNT1 = 0x0000;
	TCCR1B |= 0x02;
}

int main(void)
{	
	TCCR0A = 0x02; // disable PWM outputs, set up for clear time on compare match mode (CTC)
	TCCR0B = 0x00; // write last bit to 0 for CTC mode
	TCNT0  = 0x00; // clear timer count register
	TIMSK0 = 0x07; // trigger interrupts on match of A and B
	OCR0A  = 0x64; // set timer count to 100
	motor_speed = 0;
	prev_motor_speed = 0;
	OCR0B = motor_speed;
	
	TCCR1A = 0x00; // set up timer to use CTC mode
	TCCR1B = 0x08; // set up CTC mode, also set up input capture to capture rising edge
	TCCR1C = 0x00; // not using force output compare, so set to zero
	TCNT1 = 0x0000; // clear timer count
	OCR1A = 0x4E20; // set up top of counter to be 20000 (20 ms at 8 MHz w/ divide by 8)
	TIMSK1 = 0x02; // set up interrupts on timer = OCR1A
	
	servo_pulse_flag = 0;
	servo_timeout_flag = 0;
	servo_pwm_time = 0;
	
	DDRD  = 0xC3; // set direction to out for PD0, PD1, PD6, and PD7
	PORTD = 0x00; // set output value to 0
	
	EICRA = 0x03; // set up pin change interrupt 0 on rising edge
	EIMSK = 0x01; // enable interrupt in interrupt mask
	
	PORTD = 0x41; // charge both capacitors
	_delay_ms(10);
	
	TCCR0B |= 0x02; // enable counter with prescaler of 8, switching frequency of around 10 kHz at 8 MHz clock
	TCCR1B |= 0x02; // enable counter with prescaler of 8, 20 ms period at 20 MHz clock
	
	DDRC = 0x1C;
	PORTC = 0x04;
	
	sei();
	
    while (1) {
		if(servo_pulse_flag) {
			servo_pulse_flag = 0;
			prev_motor_speed = motor_speed;
			// allow 10% variance either direction
			if(servo_pwm_time < 2200 && servo_pwm_time > 800) {
				if(servo_pwm_time > 2000) servo_pwm_time = 2000;
				if(servo_pwm_time < 1000) servo_pwm_time = 1000;
				if(servo_pwm_time > 1425 && servo_pwm_time < 1575) servo_pwm_time = 1500;
				motor_speed = (servo_pwm_time - 1500) / 5;
			} else {
				motor_speed = 0;
			}
		}
		
		if(servo_timeout_flag) {
			servo_timeout_flag = 0;
			prev_motor_speed = motor_speed;
			motor_speed = 0;
		}
		
		OCR0B = abs(motor_speed);
		
		if(motor_speed > 0 && prev_motor_speed <= 0) {
			PORTC = 0x10; // turn LED green
		} else if(motor_speed < 0 && prev_motor_speed >= 0) {
			PORTC = 0x08; // turn LED red
		} else if(motor_speed == 0 && prev_motor_speed != 0) {
			PORTC = 0x04; // turn LED blue
		}
    }
}

