/* --------------------------------------------------------------------

  Random Dice rolling program for Arduino Due and Duinotech XC-4454 LC Controller


  By Luke Mathes, 19 March 2016

  Uses modified example code from Marc Alexander
      http://www.freetronics.com/products/lcd-keypad-shield
  -----------------------------------------------------------------------

  Program asks user how many dice they would like to roll and what dice are to be rolled. Dice
  included at D4, D6, D10, D12, D20 and D100, along with the ability to draw a random playing card. Up to
  five dice or cards may be drawn on each roll.

  The player is given the below screen. Left and right buttons move the "*" cursor between the first
  number, the dice to be rolled, and the "Roll" option. Pressing select or up on a number or die
  will increase it until it loops around from maximum. Pressing down will reduce it until it loops
  from minimum. Pressing select on "Roll" will change the numbers on the second row.

       ------------------
       |     3 D6  *Roll|
       |5 1 6           |
       ------------------

  The worst case scenario is if the user selects a card or D100 to be rolled, five numbers, and all
  of the numbers are 3 characters such as with 100 for a D100 or a 10 of any suit for a card. In this
  case, the last card or roll will be placed at the start of the first row. If last number will not
  go off screen, it is still placed at end of screen.

       ------------------
       |10S *5 Card Roll|
       |10H 10D 10C 10D |
       ------------------

       ------------------
       |    *5 Card Roll|
       |10 10D 10C 10D 1|
       ------------------

  -----------------------------------------------------------------------

  The LCD and keypad shield for Arduino uses the following pins to drive the LCD and button inputs.

    A0: Buttons, analog input from voltage ladder
    D4: LCD bit 4
    D5: LCD bit 5
    D6: LCD bit 6
    D7: LCD bit 7
    D8: LCD RS
    D9: LCD E
    D3: LCD Backlight (high = on, also has pullup high so default is on)

  As the input from the buttons is based on the voltage ladder passed into the ADC, the corresponding
  voltage levels and a hysteresis level must be defined.

  RIGHT:    0 @ 10 bit
  UP:   154 @ 10 bit
  DOWN: 392 @ 10 bit
  LEFT: 626 @ 10 bit
  SELECT: 972 @ 10 bit

  HYSTERESIS: +- 10 bits


  -------------------------------------------------------------------- */


/*--------------------------------------------------------------------------------------
  Includes
  --------------------------------------------------------------------------------------*/
#include <Wire.h>
#include <LiquidCrystal.h>   // include LCD library
#include <stdint.h>
/*--------------------------------------------------------------------------------------
  Defines
  --------------------------------------------------------------------------------------*/
// Pins in use
#define BUTTON_ADC_PIN           A0  // A0 is the button ADC input
#define LCD_BACKLIGHT_PIN         3  // D3 controls LCD backlight
// ADC readings expected for the 5 buttons on the ADC input
#define RIGHT_10BIT_ADC           0  // right
#define UP_10BIT_ADC            154  // up
#define DOWN_10BIT_ADC          392  // down
#define LEFT_10BIT_ADC          626  // left
#define SELECT_10BIT_ADC        972  // right
#define BUTTONHYSTERESIS         10  // hysteresis for valid button sensing window
//return values for ReadButtons()
#define BUTTON_NONE               0  // 
#define BUTTON_RIGHT              1  // 
#define BUTTON_UP                 2  // 
#define BUTTON_DOWN               3  // 
#define BUTTON_LEFT               4  // 
#define BUTTON_SELECT             5  // 
//some example macros with friendly labels for LCD backlight/pin control, tested and can be swapped into the example code as you like
#define LCD_BACKLIGHT_OFF()     digitalWrite( LCD_BACKLIGHT_PIN, LOW )
#define LCD_BACKLIGHT_ON()      digitalWrite( LCD_BACKLIGHT_PIN, HIGH )
#define LCD_BACKLIGHT(state)    { if( state ){digitalWrite( LCD_BACKLIGHT_PIN, HIGH );}else{digitalWrite( LCD_BACKLIGHT_PIN, LOW );} }
// Mask for debouncing 0b00111111
#define DEBOUNCE_BIT_MASK 0x3f
// Screen location constants
#define SCREEN_LOCATION_NUMBER 5
#define SCREEN_LOCATION_TYPE 7
#define SCREEN_CURSOR_NUMBER 4
#define SCREEN_CURSOR_TYPE 6
#define SCREEN_CURSOR_ROLL 11
// For main loop state machine
#define NUMBER_STATE 0
#define TYPE_STATE 1
#define ROLL_STATE 2
#define STATE_LEFT -1
#define STATE_RIGHT 1
// For dice type variable
#define DICE_D4 0
#define DICE_D6 1
#define DICE_D10 2
#define DICE_D12 3
#define DICE_D20 4
#define DICE_D100 5
#define DICE_CARD 6
#define TYPE_MAXIMUM 6
#define ROLL_MAXIMUM 4
/*--------------------------------------------------------------------------------------
  Variables
  --------------------------------------------------------------------------------------*/
// Global loop state variable. Read from main loop and selection_input, only changed from state_handler().
int8_t loop_state = NUMBER_STATE;

// Dice type values, used for printing to screen and as maximum for generating random numbers
// 13 is only used for cards and never printed
const byte dice_type_values[] = { 4, 6, 10, 12, 20, 100, 13};

// String and character arrays used for printing cards
const char * const cards_strings[] = { "F", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
const char card_suit[] = { 'H', 'S', 'C', 'D' };

// Cursor locations used for easy lookup in state_handler function
const byte cursor_locations[] = { SCREEN_CURSOR_NUMBER, SCREEN_CURSOR_TYPE, SCREEN_CURSOR_ROLL };
/*--------------------------------------------------------------------------------------
  Init the LCD library with the LCD pins to be used
  --------------------------------------------------------------------------------------*/
LiquidCrystal lcd( 8, 9, 4, 5, 6, 7 );   //Pins for the freetronics 16x2 LCD shield. LCD: ( RS, E, LCD-D4, LCD-D5, LCD-D6, LCD-D7 )
/*--------------------------------------------------------------------------------------
  setup()
  Called by the Arduino framework once, before the main loop begins
  --------------------------------------------------------------------------------------*/
void setup() {
  //button adc input
  pinMode( BUTTON_ADC_PIN, INPUT );         //ensure A0 is an input
  digitalWrite( BUTTON_ADC_PIN, LOW );      //ensure pullup is off on A0
  //lcd backlight control
  digitalWrite( LCD_BACKLIGHT_PIN, HIGH );  //backlight control pin D3 is high (on)
  pinMode( LCD_BACKLIGHT_PIN, OUTPUT );     //D3 is an output
  //set up the LCD number of columns and rows:
  lcd.begin( 16, 2 );
  // Print initial screen
  lcd.setCursor( 0, 0 );
  lcd.print( "    *1 D4   Roll" );
}

/*--------------------------------------------------------------------------------------
  loop()
  Called by Ardiono framework after setup(). Effectively main() and will loop endlessly.
  --------------------------------------------------------------------------------------*/
void loop() {
  static int8_t number_rolls = 0;
  static int8_t dice_type = DICE_D4;

  switch (loop_state)
  {
    // Run appropriate function depending on current state
    // State is changed through state_handler function called from each below function
    case NUMBER_STATE:
      selection_input(&number_rolls, ROLL_MAXIMUM);
      break;
    case TYPE_STATE:
      selection_input(&dice_type, TYPE_MAXIMUM);
      break;
    case ROLL_STATE:
      roll_function( number_rolls, dice_type );
      break;
    default:
      clear_lcd();
      lcd.print("ERROR, STATE");
      while (1);
  }
}


/*--------------------------------------------------------------------------------------
  selection_input()
  Description:
    Reads user input from buttons and depending on input either increases/decreases the passed value,
    or changes state. Takes a pointer to the variable to be edited and a maximum value so below zero and
    above maximum values can be handled.
    Function handles both dice type and number of dice inputs, reading the global loop_state to determine
    where to print.
  Inputs:
    Pointer to value to be changed.
    Maximum value constant associated with variable being changed.
    Reads global loop_state.
    User input through buttons and ReadButtons() function.
  Outputs:
    Changes passed pointer value depending on user input.
    Prints to screen.
    Can call state_handler to change global loop_state.
  --------------------------------------------------------------------------------------*/
void selection_input( int8_t * changed_var, const byte maximum_val)
{
  switch (ReadButtons())
  {
    case BUTTON_UP:
    case BUTTON_SELECT:
      // Increment by 2 as it will fall through
      *changed_var += 2;
    case BUTTON_DOWN:
      (*changed_var)--;

      // Check it changed_var has gone above or below the limits, and if so loop it around
      if ( 0 > *changed_var )
      {
        *changed_var = maximum_val;
      }
      if ( maximum_val < *changed_var )
      {
        *changed_var = 0;
      }

      // Function only called in number_state or type_state, so check global
      if (NUMBER_STATE == loop_state)
      {
        // Print numbers to number location on screen
        lcd.setCursor( SCREEN_LOCATION_NUMBER, 0 );
        // Number stored in
        lcd.print((*changed_var) + 1);
        return;
      }
      // Must be printing dice or card type
      // Clear screen first to remove additional characters
      lcd.setCursor( SCREEN_LOCATION_TYPE, 0 );
      lcd.print("    ");
      lcd.setCursor( SCREEN_LOCATION_TYPE, 0 );
      if ( DICE_CARD == *changed_var)
      {
        lcd.print("Card");
        return;
      }
      lcd.print("D");
      lcd.print(dice_type_values[*changed_var]);
      return;

    // If user presses left or right, pass to state_handler
    case BUTTON_LEFT:
      state_handler(STATE_LEFT);
      return;
    case BUTTON_RIGHT:
      state_handler(STATE_RIGHT);
      return;

    // Most loops of function will come here and do nothing
    case BUTTON_NONE:
    default:
      return;
  }

}

/*--------------------------------------------------------------------------------------
  roll_function()
  Description:
    Handlers user input when system is in "roll" state. Only performs two tasks; changing state
    on either a left or right button press, and running the rolling numbers function on a select
    button press.
  Inputs:
    Number of rolls and dice type taken from the main loop.
    User input through ReadButtons().
  Outputs:
    Changes state through state_handler().
    Calls print_randoms() and passes arguments
  --------------------------------------------------------------------------------------*/
void roll_function( const int8_t number_rolls, const int8_t dice_type )
{
  switch (ReadButtons())
  {
    case BUTTON_SELECT:
      // Run main random number printing function only when select is pressed
      print_randoms( number_rolls, dice_type);
      return;
    // If user presses left or right, pass to state_handler
    case BUTTON_LEFT:
      state_handler(STATE_LEFT);
      return;
    case BUTTON_RIGHT:
      state_handler(STATE_RIGHT);
      return;
    // Up and down do not do anything in this state
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_NONE:
    default:
      return;
  }
}


/*--------------------------------------------------------------------------------------
  print_randoms()
  Description:
    Prints the user selected dice/card type the user selected amount of times. Function keeps
    track of the location of cursor offset by 3 or 1 digit numbers and if a value would run
    off the end of the screen, it instead is printed in the top-left.
  Inputs:
    Number of random values to generate and what type to generate.
  Outputs:
    Clears old random values from LCD screen and prints new values.
  --------------------------------------------------------------------------------------*/
void print_randoms( const int8_t number_rolls, const int8_t dice_type)
{
  // Randomise seed based on microseconds since startup on every request
  randomSeed(micros());
  // Store value locally to prevent repeated lookups
  byte dice_type_max = dice_type_values[dice_type];
  byte random_temp_store = 0;
  int8_t offset = 0;
  // Random function uses exclusive max, increment here to save mathematics on every call
  dice_type_max++;
  // Clear old random values and set cursor
  lcd.setCursor( 0, 0 );
  lcd.print("   ");
  lcd.setCursor( 0, 1 );
  lcd.print("                ");
  lcd.setCursor( 0, 1 );
  // number_rolls is actually 1 less than the number displayed and the number rolled, so >= is used
  for ( int i = 0; number_rolls >= i; i++)
  {
    if ( 14 != dice_type_max )    // Not a card
    {
      // Print random number up to maximum
      random_temp_store = random( 1, dice_type_max);
      // Increase offset if value is 100 or a 10 card, and decrease if a single digit (number below 10)
      if (100 == random_temp_store)
      {
        offset++;
      }
      if (10 > random_temp_store)
      {
        offset--;
      }
      // Move cursor to top row if number will run over end of screen
      if (( 4 <= i ) && (3 <= offset))
      {
        lcd.setCursor( 0, 0 );
      }
      lcd.print(random_temp_store);
    }
    else
    {
      // Print random suit, followed by random card number
      // Increase offset if value is 100 or a 10 card, and decrease if a single digit (number below 10)
      random_temp_store = random( 1, dice_type_max);
      if (10 == random_temp_store)
      {
        offset++;
      }
      // Move cursor to top row if number will run over end of screen
      if (( 4 <= i ) && (3 <= offset))
      {
        lcd.setCursor( 0, 0 );
      }
      lcd.print(card_suit[random(4)]);
      lcd.print(cards_strings[random_temp_store]);
    }
    lcd.print(" ");
  }
}




/*--------------------------------------------------------------------------------------
  state_handler()
  Description:
    Changes global loop_state and moves cursor to appropriate location. Function uses a
    constant array "cursor_locations" containing cursor locations to both clear and place
    cursors rather than branching.
  Inputs:
    Change_state constant which is either a one or negative one.
  Outputs:
    Clears cursors and prints new cursor to LCD screen.
    Changes loop_state to new value.
  --------------------------------------------------------------------------------------*/
void state_handler( byte change_state )
{
  // Clear all cursor locations
  for ( byte i = 0; 3 > i; i++ )
  {
    lcd.setCursor( cursor_locations[i], 0 );
    lcd.print(" ");
  }
  // Change state is either zero or one to indicate left or right
  loop_state += change_state;
  // If state has gone out of range due to change_state modification, loop it around into range
  if ( 0 > loop_state )
  {
    loop_state = 2;
  }
  if ( 2 < loop_state )
  {
    loop_state = 0;
  }

  // Print cursor to screen at new location. Constant array with locations is used to save branching
  lcd.setCursor( cursor_locations[loop_state], 0 );
  lcd.print("*");
}

/*--------------------------------------------------------------------------------------
  clear_lcd()
  Simple function that clears the LCD screen. Can be called from anywhere to remove old text from screen.
  --------------------------------------------------------------------------------------*/
void clear_lcd() {
  lcd.setCursor( 0, 0 );
  lcd.print( "                " );
  lcd.setCursor( 0, 1 );
  lcd.print( "                " );
}

/*--------------------------------------------------------------------------------------
  ReadButtons()
  Description:
    Detect the button pressed and return the value. Only returns a value on a rising edge once debounced.
  Inputs:
    User input through buttons connected in a voltage ladder to A0 ADC.
  Outputs:
    Returns a byte indicating the button pressed.
  --------------------------------------------------------------------------------------*/
byte ReadButtons()
{
  unsigned int buttonVoltage;
  byte button = BUTTON_NONE;   // return no button pressed if the below checks don't write to btn
  static byte buttonWas          = BUTTON_NONE;   //used by ReadButtons() for detection of button events
  static byte debounce_detection = 0;

  //read the button ADC pin voltage
  buttonVoltage = analogRead( BUTTON_ADC_PIN );
  //sense if the voltage falls within valid voltage windows
  if ( buttonVoltage < ( RIGHT_10BIT_ADC + BUTTONHYSTERESIS ) )
  {
    button = BUTTON_RIGHT;
  }
  else if (   buttonVoltage >= ( UP_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( UP_10BIT_ADC + BUTTONHYSTERESIS ) )
  {
    button = BUTTON_UP;
  }
  else if (   buttonVoltage >= ( DOWN_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( DOWN_10BIT_ADC + BUTTONHYSTERESIS ) )
  {
    button = BUTTON_DOWN;
  }
  else if (   buttonVoltage >= ( LEFT_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( LEFT_10BIT_ADC + BUTTONHYSTERESIS ) )
  {
    button = BUTTON_LEFT;
  }
  else if (   buttonVoltage >= ( SELECT_10BIT_ADC - BUTTONHYSTERESIS )
              && buttonVoltage <= ( SELECT_10BIT_ADC + BUTTONHYSTERESIS ) )
  {
    button = BUTTON_SELECT;
  }

  // Debouncing. Shift bits and set least significant bit to high if button is still changed
  debounce_detection << 1;
  if (buttonWas != button)
  {
    debounce_detection++;
    delay(1);
  }

  // When six straight new reads are detected, pass the new value out of function and change buttonWas
  if ( DEBOUNCE_BIT_MASK == debounce_detection)
  {
    debounce_detection = 0;
    buttonWas = button;
    return ( button );
  }

  // If not a rising edge debounce, return nothing
  return ( BUTTON_NONE );
}
