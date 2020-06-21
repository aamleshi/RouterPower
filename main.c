#include "main.h" 

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>

//http://web.alfredstate.edu/faculty/weimandn/programming/lcd/ATmega328/LCD_code_gcc_4f.html

// -------- Global Variables --------- //
#define LED PD6
#define LEDPORT PORTD
#define LEDDDR DDRD

#define DEBUG       PD7
#define DEBUGPORT   PORTD
#define DEBUGDDR    DDRD
#define DEBUG2      PB0
#define DEBUG2PORT  PORTB
#define DEBUG2DDR   DDRB

#define DEBOUNCETIME 2000
#define BLINKTIME 100
#define DISABLEDELAY 500
#define FASTMESSAGE 3000

#define BUTTON1 PB3
#define BUTTON2 PB2
#define BUTTON3 PB1

#define BUTTON1PIN PINB
#define BUTTON2PIN PINB 
#define BUTTON3PIN PINB

#define BUTTON1PORT PORTB
#define BUTTON2PORT PORTB 
#define BUTTON3PORT PORTB

#define BUTTON1DDR DDRB 
#define BUTTON2DDR DDRB
#define BUTTON3DDR DDRB

#define RELAY     PD1
#define RELAYPORT PORTD
#define RELAYDDR  DDRD 


#define LCD_RW  PC4
#define LCD_RS  PC5
#define LCD_E   PC3
#define LCD_D4  PC2
#define LCD_D5  PC1
#define LCD_D6  PC0
#define LCD_D7  PB5

#define LCD_RW_DDR DDRC
#define LCD_RS_DDR DDRC
#define LCD_E_DDR  DDRC
#define LCD_D4_DDR DDRC
#define LCD_D5_DDR DDRC
#define LCD_D6_DDR DDRC
#define LCD_D7_DDR DDRB

#define LCD_RW_PORT PORTC
#define LCD_RS_PORT PORTC
#define LCD_E_PORT  PORTC
#define LCD_D4_PORT PORTC
#define LCD_D5_PORT PORTC
#define LCD_D6_PORT PORTC
#define LCD_D7_PORT PORTB

#define LCD_D7_PIN PINB // Busy flag

#define LCD_LINE1 0x00
#define LCD_LINE2 0x40

// LCD instructions
#define LCD_CLEAR           0b00000001          // replace all characters with ASCII 'space'
#define LCD_HOME            0b00000010          // return cursor to first position on first line
#define LCD_ENTRYMODE       0b00000110          // shift cursor from left to right on read/write
#define LCD_DISPLAYOFF      0b00001000          // turn display off
#define LCD_DISPLAYON       0b00001100          // display on, cursor off, don't blink character
#define LCD_DISPLAYONBLINK  0b00001111
#define LCD_FUNCTIONRESET   0b00110000          // reset the LCD
#define LCD_FUNCTIONSET4BIT 0b00101000           // 4-bit data, 2-line display, 5 x 7 font  
#define LCD_SETCURSOR       0b10000000          // set cursor position
#define LCD_SETBLINKON      0b00001101          // sets blink to ON
#define LCD_SETBLINKOFF     0b00001100          // sets blink to OFF
#define LCD_SETCURSORON     0b00001110          // sets cursor to ON
#define LCD_SETCURSOROFF    0b00001100          // sets cursor to OFF

//State machine defines
/*
enum States{standbyState, setTimeState, setAlarmState, disableState, enableState};
enum MenuStates{setTimeMenu, setAlarmMenu, disableMenu};
enum Days{sun, mon, tues, weds, thurs, fri, sat};
enum EditTimeState{editHour, editMin, editAccept};
*/
//STATES
#define STANDBY_STATE         0
#define SET_TIME_STATE        1
#define SET_ALARM_STATE       2
#define SET_DOWNTIME_STATE    3
#define SET_DISABLE_STATE     4



//MENUSTATES
#define SET_TIME_MENU       0
#define SET_ALARM_MENU      1
#define SET_DOWNTIME_MENU   2
#define SET_DISABLE_MENU    3

//EDIT_TIME_STATES
#define EDIT_HOUR_TIMESTATE   0
#define EDIT_MIN_TIMESTATE    1

struct time {
  volatile uint8_t hour;
  volatile uint8_t minute;
};

// Function Defs //
void lcd_write4(uint8_t);
void lcd_init(void);
void lcd_block_bf(void);
void lcd_write_instruction(uint8_t);
void lcd_write_half_instruction(uint8_t);
void lcd_writeChar(uint8_t);
void lcd_writeStr(uint8_t *, uint8_t);
void lcd_moveCursor(uint8_t, uint8_t);
void lcd_toggleBlink(uint8_t);
void lcd_toggleCursor(uint8_t);
void lcd_clear();
void lcd_writeTop(uint8_t str[]);
void lcd_writeBot(uint8_t str[]);
void incrementMenu();
void decrementMenu();
void lcd_displayStandby(struct time *);
void lcd_displaySetTime(struct time *);
void lcd_displaySetAlarm(struct time *);
void lcd_displaySetDowntime(struct time *);
void lcd_displayToggleDisable();
void lcd_displayDisableBarFill(uint8_t);
void lcd_displayDisableSucess();
void lcd_displayDisableFailure();
void lcd_displayEnableSucess();
uint8_t button1();
uint8_t button2();
uint8_t button3();
uint8_t button1_stream();
void minutePassed(struct time *t);
int incrementMinute(struct time *t);
void incrementHour(struct time *t);
int decrementMinute(struct time *t);
void decrementHour(struct time *t);
void setTimestring(char *timestring, struct time *t);
uint8_t timesEqual(struct time *, struct time *);
struct time *timeDif(struct time*, struct time *, struct time*);
struct time *timeSum(struct time*, struct time *, struct time*);
uint8_t timeGreater(struct time *, struct time *);
uint8_t timeBetween(struct time*, struct time*, struct time*);
void blinkDEBUG(uint8_t);
void blinkDEBUG2(uint8_t);
void blinkDEBUGs(uint8_t);



//=======GLOBAL VARIABLES========//
struct time clockTime = {0, 0};
struct time powerOffTime = {23, 59};
struct time powerOnTime = {8, 0};
struct time powerOffInterval = {8,0};
struct time countdownTime = {0, 0};
struct time zeroTime = {23,60};
struct time timeBuffer = {23,59};

volatile uint8_t powerState = 1;
volatile uint8_t change = 1;
volatile uint8_t system_state = SET_TIME_STATE;
volatile uint8_t disable = 1;
volatile uint8_t seconds = 0;

char editTimeState = EDIT_HOUR_TIMESTATE;
char menustate = SET_TIME_MENU;
//=======GLOBAL VARIABLES========//


// -------- Functions --------- //

static inline void initTimer1(void){
  //useful link to register explaination
  //https://sites.google.com/site/qeewiki/books/avr-guide/timers-on-the-atmega328
  OCR1A = 15625; //Set output compare
  TCCR1B |= (1 << WGM12);               // CTC mode
  TIMSK1 |= (1 << OCIE1A);              //Set interrupt on compare match
  TCCR1B |= (1 << CS11) | (1 << CS10);  // set prescaler to 64 and start the timer
}

ISR (TIMER1_COMPA_vect)
{
  // action to be done every 1 sec
  if (seconds < 60){
    change = 1;
    seconds += 1;
  } else {
    seconds = 0;
    minutePassed(&clockTime);
  }
  change = 1;
  if (bit_is_clear(LEDPORT, LED)){
    LEDPORT |= (1<<LED);
  } else {
    LEDPORT &= ~(1<<LED);
  }
  timeSum(&powerOffTime, &powerOffInterval, &powerOnTime);
  if (timeBetween(&clockTime, &powerOnTime, &powerOffTime)){
    DEBUGPORT |= (1<<DEBUG);
    DEBUG2PORT &= ~(1<<DEBUG2);
    powerState = 1;
  } else {
    DEBUGPORT &= ~(1<<DEBUG);
    DEBUG2PORT |= (1<<DEBUG2);
    if (!disable){
      powerState = 0;
    }
  }
  if (timesEqual(&clockTime, &powerOnTime)) {
    disable = 0;
  }

  if (powerState){
    RELAYPORT &= ~(1<<RELAY);
  } else {
    RELAYPORT |= (1<<RELAY);
  }
}

int main(void){
  // -------- Inits --------- //
  LEDDDR |= (1<<LED);
  DEBUGDDR |= (1<<DEBUG);
  DEBUG2DDR |= (1<<DEBUG);

  BUTTON1DDR &= ~(1<<BUTTON1);
  BUTTON2DDR &= ~(1<<BUTTON2);
  BUTTON3DDR &= ~(1<<BUTTON3);
  BUTTON1PORT |= (1<<BUTTON1);
  BUTTON2PORT |= (1<<BUTTON2);
  BUTTON3PORT |= (1<<BUTTON3);

  LCD_RS_DDR |= (1<<LCD_RS);
  LCD_E_DDR  |= (1<<LCD_E);
  LCD_RW_DDR |= (1<<LCD_RW);
  LCD_D4_DDR |= (1<<LCD_D4);
  LCD_D5_DDR |= (1<<LCD_D5);
  LCD_D6_DDR |= (1<<LCD_D6);
  LCD_D7_DDR |= (1<<LCD_D7);

  RELAYDDR |= (1<<RELAY);

  uint8_t disableProgress = 0;

  initTimer1();
  lcd_init();


  // ------ Event loop ------ //
  while (1){
    if (system_state == STANDBY_STATE){
      if (change){
        if (powerState){
          timeDif(&clockTime, &powerOffTime, &countdownTime);
        } else {
          timeDif(&clockTime, &powerOnTime, &countdownTime);
        }
        lcd_displayStandby(&countdownTime);
        change = 0;
      }
      if (button1()){
        change = 1;
        incrementMenu();
      }
      if (button2()){
        change = 1;
        decrementMenu();
      }
      if (button3()){
        change = 1;
        if (menustate == SET_TIME_MENU){
          cli();
          editTimeState = EDIT_HOUR_TIMESTATE;
          system_state += SET_TIME_STATE;
        } else if (menustate == SET_ALARM_MENU){
          editTimeState = EDIT_HOUR_TIMESTATE;
          system_state += SET_ALARM_STATE;
        } else if (menustate == SET_DOWNTIME_MENU){
          editTimeState = EDIT_HOUR_TIMESTATE;
          system_state  += SET_DOWNTIME_STATE;
        } else if (menustate == SET_DISABLE_MENU){
          disableProgress = 0;
          editTimeState = EDIT_HOUR_TIMESTATE;
          system_state  += SET_DISABLE_STATE;
        }
      }
    } else if (system_state == SET_TIME_STATE){
      if (change){
        lcd_displaySetTime(&clockTime);
        change = 0;
      }
      if (button1()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          incrementHour(&clockTime);
        } else if (editTimeState ==EDIT_MIN_TIMESTATE){
          incrementMinute(&clockTime);
        }
      }
      if (button2()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          decrementHour(&clockTime);
        } else if (editTimeState ==EDIT_MIN_TIMESTATE){
          decrementMinute(&clockTime);
        }
      }

      if (button3()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          editTimeState = EDIT_MIN_TIMESTATE;
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          sei();
          system_state = STANDBY_STATE;
        } 
      }
    } else if (system_state == SET_ALARM_STATE){
      if (change){
        lcd_displaySetAlarm(&powerOffTime);
        change = 0;
      }

      if (button1()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          incrementHour(&powerOffTime);
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          incrementMinute(&powerOffTime);
        }
      }

      if (button2()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          decrementHour(&powerOffTime);
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          decrementMinute(&powerOffTime);
        }
      }

      if (button3()){ 
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          editTimeState = EDIT_MIN_TIMESTATE;
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          system_state = STANDBY_STATE;
        }
      }
    } else if (system_state == SET_DOWNTIME_STATE){
      if (change){
        lcd_displaySetDowntime(&powerOffInterval);
        change = 0;
      }

      if (button1()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          incrementHour(&powerOffInterval);
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          incrementMinute(&powerOffInterval);
        }
        timeSum(&powerOffTime, &powerOffInterval, &powerOnTime);
      }

      if (button2()){
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          decrementHour(&powerOffInterval);
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          decrementMinute(&powerOffInterval);
        }
        timeSum(&powerOffTime, &powerOffInterval, &powerOnTime);
      }

      if (button3()){ 
        change = 1;
        if (editTimeState == EDIT_HOUR_TIMESTATE){
          editTimeState = EDIT_MIN_TIMESTATE;
        } else if (editTimeState == EDIT_MIN_TIMESTATE){
          system_state = STANDBY_STATE;
        }
      }
    } else if (system_state == SET_DISABLE_STATE){
      if (change){
        lcd_displayToggleDisable(disableProgress);
        change = 0;
      }
      if (button1_stream()){
        if (!disable){
          while(1){
           if (!button1_stream()){
              disableProgress = 0;
              lcd_displayDisableFailure();
              _delay_ms(FASTMESSAGE);
              system_state = STANDBY_STATE;
              break;
            } 
            if (disableProgress < 14){
              disableProgress += 1;
              _delay_ms(DISABLEDELAY);
              lcd_displayDisableBarFill(disableProgress);
            } else {
              lcd_displayDisableSucess();
              _delay_ms(FASTMESSAGE);
              disable += 1;
              if (disable){
                blinkDEBUG(3);
              }
              change = 1;
              system_state = STANDBY_STATE;
              break;
            }
          }
        } else {
          disable = 0;
          lcd_displayEnableSucess();
          _delay_ms(FASTMESSAGE);
          system_state = STANDBY_STATE;
        }
      } 
      
      if (button3()){
        change = 1;
        system_state = STANDBY_STATE;
      }
    }
  }                                                 
  return (0);                            
}

void blinkDEBUG(uint8_t t){
  DEBUGPORT |= (1<<DEBUG);
  for (int i = 0; i < t; i++){
    _delay_ms(BLINKTIME);
  }
  DEBUGPORT &= ~(1<<DEBUG);
}

void blinkDEBUG2(uint8_t t){
  DEBUG2PORT |= (1<<DEBUG2);
  for (int i = 0; i < t; i++){
    _delay_ms(BLINKTIME);
  }
  DEBUG2PORT &= ~(1<<DEBUG2);
}

void blinkDEBUGs(uint8_t t){
  DEBUGPORT |= (1<<DEBUG);
  DEBUG2PORT |= (1<<DEBUG2);
  for (int i = 0; i < t; i++){
    _delay_ms(BLINKTIME);
  }
  DEBUGPORT &= ~(1<<DEBUG);
  DEBUG2PORT &= ~(1<<DEBUG2);
}

uint8_t timesEqual(struct time *t1, struct time * t2){
  if ((t1->hour == t2->hour) && (t1->minute == t2->minute)){
    return 1;
  }
  return 0;
}

struct time *timeSum(struct time *t1, struct time *t2, struct time *deltaT){
  /*Write the sum of t1 and t2 into deltaT*/
  deltaT->hour = (t1->hour + t2->hour)%24;
  deltaT->minute = t1->minute + t2->minute;
  if (deltaT->minute > 59){
    deltaT->minute -= 60;
    deltaT->hour += 1;
  }
  return deltaT;
}

uint8_t timeGreater(struct time *t1, struct time *t2){
  //Note that this is greater or equal
  if ((t1->hour > t2->hour) || ((t1->hour == t2->hour)&&(t1->minute >= t2->minute))){
    return 1;
  }
  return 0;
}

struct time *timeDif(struct time *t1, struct time *t2, struct time *deltaT){
  /* write time between t1 and t2 into deltaT*/ 
  if (timeGreater(t2, t1)){
    if (t1->minute > t2->minute){
      deltaT->hour = t2->hour - t1->hour - 1;
      deltaT->minute = 60 + t2->minute - t1->minute;
    } else {
      deltaT->hour = t2->hour - t1->hour;
      deltaT->minute = t2->minute - t1->minute;
    }
  } else {
    timeSum(t2, timeDif(t1, &zeroTime, &timeBuffer), deltaT);
  }
  return deltaT;
}

uint8_t timeBetween(struct time *testTime, struct time *timeOrigin, struct time *timeEnd){
  struct time timeToEnd = {0,0};
  struct time timeToOrigin = {0.0};
  timeDif(testTime, timeEnd, &timeToEnd);
  timeDif(testTime, timeOrigin, &timeToOrigin);
  if (timesEqual(testTime, timeEnd)){
    return 0;
  }
  if (timeGreater(&timeToOrigin, &timeToEnd)){
    return 1;
  }
  return 0;
}




void incrementMenu(){
  switch (menustate){
    case SET_TIME_MENU:
      menustate = SET_ALARM_MENU;
      return;
    case SET_ALARM_MENU:
      menustate = SET_DOWNTIME_MENU;
      return;
    case SET_DOWNTIME_MENU:
      menustate = SET_DISABLE_MENU;
      return;
    case SET_DISABLE_MENU:
      menustate = SET_TIME_MENU;
      return;
    default:
      //something weird
      return;
  }
}

void decrementMenu(){
  switch (menustate){
    case SET_TIME_MENU:
      menustate = SET_DISABLE_MENU;
      return;
    case SET_ALARM_MENU:
      menustate = SET_TIME_MENU;
      return;
    case SET_DOWNTIME_MENU:
      menustate = SET_ALARM_MENU;
      return;
    case SET_DISABLE_MENU:
      menustate = SET_DOWNTIME_MENU;
      return;
    default:
      //something weird
      return;
  }
}




void setTimestring(char *timestring, struct time *t){
  sprintf(timestring, "%02d:%02d", t->hour, t->minute);
}

void minutePassed(struct time *t){
  if (incrementMinute(t)){
    incrementHour(t);
  }
}

int incrementMinute(struct time *t){
  if (t->minute == 59){
    t->minute = 0;
    return 1;
  }
  t->minute += 1;
  return 0;
}

int decrementMinute(struct time *t){
  if (t->minute == 0){
    t->minute = 59;
    return 1;
  }
  t->minute -= 1;
  return 0;
}

void incrementHour(struct time *t){
  if (t->hour == 23){
    t->hour = 0;
  } else {
    t->hour += 1;
  }
}

void decrementHour(struct time *t){
  if (t->hour == 0){
    t->hour = 23;
  } else {
    t->hour -= 1;
  }
}

uint8_t button1(){
  if (bit_is_clear(BUTTON1PIN, BUTTON1)){
    _delay_us(DEBOUNCETIME);
    if (bit_is_clear(BUTTON1PIN, BUTTON1)){
      while (bit_is_clear(BUTTON1PIN, BUTTON1)){
        _delay_ms(1);  
      }
      return 1;
    }
  } 
  return 0;
}

uint8_t button2(){
  if (bit_is_clear(BUTTON2PIN, BUTTON2)){
    _delay_us(DEBOUNCETIME);
    if (bit_is_clear(BUTTON2PIN, BUTTON2)){
      while (bit_is_clear(BUTTON2PIN, BUTTON2)){
        _delay_ms(1);  
      }
      return 1;
    }
  } 
  return 0;
}

uint8_t button3(){
  if (bit_is_clear(BUTTON3PIN, BUTTON3)){
    _delay_us(DEBOUNCETIME);
    if (bit_is_clear(BUTTON3PIN, BUTTON3)){
      while (bit_is_clear(BUTTON3PIN, BUTTON3)){
        _delay_ms(1);  
      }
      return 1;
    }
  } 
  return 0;
}

uint8_t button1_stream(){
  if (bit_is_clear(BUTTON1PIN, BUTTON1)){
    _delay_us(DEBOUNCETIME);
    if (bit_is_clear(BUTTON1PIN, BUTTON1)){
      return 1;
    }
  } 
  return 0;
}

void lcd_displayStandby(struct time *t){
  char top[16] = "                ";
  char bot[16] = "                ";
  char timestring[10];
  setTimestring(timestring, t);
  if(powerState){
    if (!disable){
      strncpy(top, "SHUTOFF IN      ", 16);
      strncpy(top+11, timestring, 5);
    } else {
      strncpy(top, "SHUTOFF DISABLED", 16);
      
    }
  } else {
    strncpy(top, "POWER IN        ", 16);
    strncpy(top+11, timestring, 5);
  }
  switch (menustate){
    case SET_TIME_MENU:
      strncpy(bot, "SET TIME->      ", 16);
      setTimestring(timestring, &clockTime);
      strncpy(bot+11, timestring, 5);
      break;
    case SET_ALARM_MENU:
      strncpy(bot, "SET ALARM->      ", 16);
      setTimestring(timestring, &powerOffTime);
      strncpy(bot+11, timestring, 5);
      break;
    case SET_DOWNTIME_MENU:
      strncpy(bot, "DOWNTIME->      ", 16);
      setTimestring(timestring, &powerOffInterval);
      strncpy(bot+11, timestring, 5);
      break;
    case SET_DISABLE_MENU:
      if (!disable){
        strncpy(bot, "DISABLE TODAY   ", 16);
      } else {
        strncpy(bot, "ENABLE TODAY    ", 16);
      }
      break;
    default:
      //something weird
      strncpy(bot, "BAD MENU        ", 16);
  }
  
  lcd_writeTop((uint8_t*) top);
  lcd_writeBot((uint8_t*) bot);
}

void lcd_displaySetTime(struct time *edit_time){
  uint8_t top[16] = "SET TIME        ";
  uint8_t bot[16] = "                ";
  switch (editTimeState){
    case EDIT_HOUR_TIMESTATE:
      strncpy((char*) bot, "           ^^   ", 16);
      break;
    case EDIT_MIN_TIMESTATE:
      strncpy((char*) bot, "              ^^", 16);
      break;
  }
  setTimestring((char*) top+11, edit_time);
  lcd_writeTop(top);
  lcd_writeBot(bot);
}

void lcd_displaySetAlarm(struct time *edit_time){
  uint8_t top[16] = "SET ALARM       ";
  uint8_t bot[16] = "                ";
  switch (editTimeState){
    case EDIT_HOUR_TIMESTATE:
      strncpy((char*) bot, "           ^^   ", 16);
      break;
    case EDIT_MIN_TIMESTATE:
      strncpy((char*) bot, "              ^^", 16);
      break;
  }
  setTimestring((char*) top+11, edit_time);
  lcd_writeTop(top);
  lcd_writeBot(bot);
}

void lcd_displaySetDowntime(struct time *edit_time){
  uint8_t top[16] = "DOWNTIME ->     ";
  uint8_t bot[16] = "                ";
  switch (editTimeState){
    case EDIT_HOUR_TIMESTATE:
      strncpy((char*) bot, "           ^^   ", 16);
      break;
    case EDIT_MIN_TIMESTATE:
      strncpy((char*) bot, "              ^^", 16);
      break;
  }
  setTimestring((char*) top+11, edit_time);
  lcd_writeTop(top);
  lcd_writeBot(bot);
}

void lcd_displayToggleDisable(){
  uint8_t top[16];
  uint8_t bot[16];
  if (!disable){
    strncpy((char*) top, "Hold To Disable ", 16);
    strncpy((char*) bot, "[              ]", 16);
  } else {
    strncpy((char*) top, "Press To Enable ", 16);
    strncpy((char*) bot, "                ", 16);
  }
  
  lcd_writeTop(top);
  lcd_writeBot(bot);
}

void lcd_displayDisableBarFill(uint8_t progress){
  lcd_moveCursor(2, progress);
  lcd_writeChar('#');
}

void lcd_displayDisableSucess(){
  uint8_t top[16] = "CUTOFF DISABLED ";
  uint8_t bot[16] = "UNTIL TOMORROW  ";
  lcd_writeTop(top);
  lcd_writeBot(bot);
}
void lcd_displayDisableFailure(){
  uint8_t top[16] = "DISABLE CANCELED";
  uint8_t bot[16] = "MEET YO' GOAL :)";
  lcd_writeTop(top);
  lcd_writeBot(bot);
}
void lcd_displayEnableSucess(){
  uint8_t top[16] = "CUTOFF ENABLED  ";
  uint8_t bot[16] = "                ";
  lcd_writeTop(top);
  lcd_writeBot(bot);
}


void lcd_write4(uint8_t nibble){
  //This expects the nibble to be in upper half of byte
  //---Set the pins---//
  if (nibble & (1<<7)){
    LCD_D7_PORT |= (1<<LCD_D7);
  } else {
    LCD_D7_PORT &= ~(1<<LCD_D7);
  }

  if (nibble & (1<<6)){
    LCD_D6_PORT |= (1<<LCD_D6);
  } else {
    LCD_D6_PORT &= ~(1<<LCD_D6);
  }

  if (nibble & (1<<5)){
    LCD_D5_PORT |= (1<<LCD_D5);
  } else {
    LCD_D5_PORT &= ~(1<<LCD_D5);
  }

  if (nibble & (1<<4)){
    LCD_D4_PORT |= (1<<LCD_D4);
  } else {
    LCD_D4_PORT &= ~(1<<LCD_D4);
  }
  //----Set the pins---//

  //----Write to enable pin----//
  LCD_E_PORT |= (1<<LCD_E);
  _delay_us(1); // implement 'Data set-up time' (80 nS) and 'Enable pulse width' (230 nS)
  LCD_E_PORT &= ~(1<<LCD_E);
  _delay_us(1); // implement 'Data hold time' (10 nS) and 'Enable cycle time' (500 nS)
  //----Write to enable pin----//
}

void lcd_write_instruction(uint8_t instruction){
  LCD_RW_PORT &= ~(1<<LCD_RW);  //Set R/W pin to write (low)
  LCD_RS_PORT &= ~(1<<LCD_RS);  //Select Instruction Register (RS Low)
  LCD_E_PORT &= ~(1<<LCD_E);  //Set Enable to low
  lcd_block_bf();   
  lcd_write4(instruction);      //write upper nibble
  lcd_block_bf();   
  lcd_write4(instruction<<4);   //write lower nibble
}

void lcd_write_half_instruction(uint8_t instruction){
  LCD_RW_PORT &= ~(1<<LCD_RW);  //Set R/W pin to write (low)
  LCD_RS_PORT &= ~(1<<LCD_RS);  //Select Instruction Register (RS Low)
  LCD_E_PORT &= ~(1<<LCD_E);  //Set Enable to low
  lcd_write4(instruction);      //write upper nibble
}

void lcd_writeChar(uint8_t c){
  lcd_block_bf();
  LCD_RW_PORT &= ~(1<<LCD_RW);  // write to LCD module (RW low)
  LCD_RS_PORT |= (1<<LCD_RS);   // select the Data Register (RS high)
  LCD_E_PORT &= ~(1<<LCD_E);  // make sure E is initially low
  lcd_write4(c);             // write the upper 4-bits of the data
  lcd_write4(c<<4);          // write the lower 4-bits of the data
}

void lcd_writeTop(uint8_t str[]){
  lcd_block_bf();
  lcd_write_instruction(LCD_SETCURSOR | LCD_LINE1);
  lcd_writeStr(str, 16);
}

void lcd_writeBot(uint8_t str[]){
  lcd_block_bf();
  lcd_write_instruction(LCD_SETCURSOR | LCD_LINE2);
  lcd_writeStr(str, 16);
}

void lcd_writeStr(uint8_t str[], uint8_t len){
  for (int i = 0; i< len; i++){
    lcd_block_bf();
    lcd_writeChar(str[i]);
  }
}


void lcd_block_bf(void){
  //Blocking function while busy flag is set//

  uint8_t bf_block = 1;
  LCD_D7_DDR &= ~(1<<LCD_D7);   // Set D7 to input
  LCD_RS_PORT &= ~(1<<LCD_RS);  // Set register select to 0
  LCD_RW_PORT |= (1<<LCD_RW);   // Set R/W to high (Read)

  while (bf_block){//Blocking Loop TODO add a watchdog lol
    LCD_E_PORT |= (1<<LCD_E); //Enable High
    _delay_us(1);             // implement 'Delay data time' (160 nS) and 'Enable pulse width' (230 nS)
    
    if (~(LCD_D7_PIN & (1<<LCD_D7))) {//If the block bit not set, turn off block
      bf_block = 0;
    }

    LCD_E_PORT &= ~(1<<LCD_E);  //Enable Low
    _delay_us(1);               // implement 'Address hold time' (10 nS), 'Data hold time' (10 nS), and 'Enable cycle time' (500 nS )

    //Dump empty nibble//
    LCD_E_PORT |= (1<<LCD_E);
    _delay_us(1);               // implement 'Delay data time' (160 nS) and 'Enable pulse width' (230 nS)
    LCD_E_PORT &= ~(1<<LCD_E);  //Enable Low
    _delay_us(1);               // implement 'Address hold time' (10 nS), 'Data hold time' (10 nS), and 'Enable cycle time' (500 nS )
  }

  LCD_RW_PORT &= ~(1<<LCD_RW);  //Set R/W to low (Write)
  LCD_D7_DDR |= (1<<LCD_D7);    // Set D7 back to output
}

void lcd_moveCursor(uint8_t line, uint8_t pos){
  if (line == 1){
    lcd_write_instruction(LCD_SETCURSOR | (LCD_LINE1+pos));
  } 
  if (line == 2){
    lcd_write_instruction(LCD_SETCURSOR | (LCD_LINE2+pos));
  }
}

void lcd_clear(){
  //clears the LCD and moves cursor back to home
  lcd_block_bf();
  lcd_write_instruction(LCD_CLEAR);
}

void lcd_toggleBlink(uint8_t blink){
  if (blink){
    lcd_write_instruction(LCD_SETBLINKON);
  } else {
    lcd_write_instruction(LCD_SETBLINKOFF);
  }
}

void lcd_toggleCursor(uint8_t cursor){
  if (cursor){
    lcd_write_instruction(LCD_SETCURSORON);
  } else {
    lcd_write_instruction(LCD_SETCURSOROFF);
  }
}

void lcd_init(void){
  LEDPORT |= (1<<LED); 
  LEDPORT &= ~(1<<LED); 
  _delay_ms(100); //Power up delay
  LCD_RS_PORT &= ~(1<<LCD_RS);  // select the Instruction Register (RS low)
  LCD_E_PORT &= ~(1<<LCD_E);    // set E Low
  LCD_RW_PORT &= ~(1<<LCD_RW);  // Set R/W to W (low)

  //Reset LCD Controller
  lcd_write_half_instruction(LCD_FUNCTIONRESET);
  _delay_ms(10);        // 4.1 mS delay (min)

  lcd_write_half_instruction(LCD_FUNCTIONRESET);
  _delay_us(200);        // 100 uS delay (min)
  

  lcd_write_half_instruction(LCD_FUNCTIONRESET);
  _delay_us(80);        // good practice?

  //Set to 4bit mode
  _delay_ms(10);//lcd_block_bf();
  lcd_write_half_instruction(LCD_FUNCTIONSET4BIT);
  _delay_us(80);        // 40 us min
  
  //--Busy flag becomes available ---//

  _delay_ms(10);//lcd_block_bf();
  lcd_write_instruction(LCD_FUNCTIONSET4BIT);

  //--Do this just like dataset says (change later if needed)--//
  _delay_ms(10);//lcd_block_bf();
  lcd_write_instruction(LCD_DISPLAYOFF);

  _delay_ms(10);//lcd_block_bf();
  lcd_write_instruction(LCD_CLEAR);

  _delay_ms(10);//lcd_block_bf();
  lcd_write_instruction(LCD_ENTRYMODE);

  //--Application Specific Setup--//
  lcd_block_bf();
  lcd_write_instruction(LCD_DISPLAYON);
}
