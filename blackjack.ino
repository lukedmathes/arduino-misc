/* --------------------------------------------------------------------

  BlackJack game for Arduino Due and Duinotech XC-4454 LC Controller


  By Luke Mathes, 11 March 2016

  Uses modified example code from Marc Alexander
      http://www.freetronics.com/products/lcd-keypad-shield
  -----------------------------------------------------------------------

  This program simulates a basic Blackjack game against a computer opponent called the "Dealer".

  Rules:

  The player is given two random cards from a standard deck and by using either the "hit" command
  to get another card or the "sit" command to stay where they are, they must try to get as close to
  a card total of 21 without going over. If the player goes over 21, they automatically lose by
  going "bust".

  After the player has decided to sit and has not gone bust, the Dealer must then do the same drawing
  cards until their total is above 16 or they go over 21. The player with the higher total without
  going over 21 is then the winner. If both players have the same total, it is a draw.

  Cards with a number on them such as a seven or a two hold the value on them. Picture cards including
  the Jack, Queen and King all hold the value of ten. The Ace has the value of either 1 or 11
  depending on which is more advantageous.

  If either player has both and Ace and a card of value 10 as their first two cards, they automatically
  win unless the opponent also has an Ace and a card of value 10, in which case it is a draw. This is
  called "Blackjack".

  If either player manages to get five cards without going over 21, they automatically sit and this
  hand can only be beaten if the opponent has Blackjack and will draw if the opponent also has five
  cards without going over 21. This is called "Five under".


  -----------------------------------------------------------------------

  The Player interacts with the game through the button inputs and the 2x16 character LCD screen.
  During the player's turn they see the following screen.

       ------------------
       |Player:*Hit *Sit|
       |13           6 7|
       ------------------

  The player can move the asterisk between the two options by pressing any of the buttons below the
  screen. When the player presses "select", the selected action will be completed.

  The number in the bottom left is the player's current total while the bottom right shows all cards
  the player has so far drawn. A 10 takes up two character spaces, but as the worst case scenario is
  only two 10s in five cards as shown below, the card display does not run into the total.

  When the player's total goes over 21, the two options Hit and Sit are replaced with "Bust" as shown
  below. If the player reaches five under or Blackjack, the top-right will instead display "Blackjack"
  or "5 under" as appropriate.

       ------------------
       |Player:     Bust|
       |30   10 3 4 3 10|
       ------------------

  If the player decides to sit while they are under 21, it then becomes the Dealer's turn. the screen
  shown by the dealer is as below with the option the dealer has decided shown above.

       ------------------
       |Dealer:      Hit|
       |11           6 5|
       ------------------

  The player is prompted to press Select at this point to advance the game at their own pace. Once the
  dealer decides to sit and the player once more presses select, they will be taken to a results screen.
  The results screen simply displays whether they won or lost and asks if they would like to play again.
  As with above, the player can move the asterisk between "Yes" and "No" using the buttons.

       ------------------
       |You win!    *Yes|
       |Play Again?  *No|
       ------------------

  If the player chooses to play again, they immediately taken to another game. If they choose not to,
  they are brought to the main menu.


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

	RIGHT:	  0 @ 10 bit
	UP:		154 @ 10 bit
	DOWN:	392 @ 10 bit
	LEFT:	626 @ 10 bit
	SELECT:	972 @ 10 bit

	HYSTERESIS:	+- 10 bits




  -------------------------------------------------------------------- */



/*--------------------------------------------------------------------------------------
  Includes
  --------------------------------------------------------------------------------------*/
#include <Wire.h>
#include <LiquidCrystal.h>   // include LCD library
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
// Defines for menu options
#define OPTION_PLAY             1
#define OPTION_HOW_TO           2
/*--------------------------------------------------------------------------------------
  Variables
  --------------------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------------------
  Init the LCD library with the LCD pins to be used
  --------------------------------------------------------------------------------------*/
LiquidCrystal lcd( 8, 9, 4, 5, 6, 7 );   //Pins for the freetronics 16x2 LCD shield. LCD: ( RS, E, LCD-D4, LCD-D5, LCD-D6, LCD-D7 )
/*--------------------------------------------------------------------------------------
  setup()
  Called by the Arduino framework once, before the main loop begins
  --------------------------------------------------------------------------------------*/
void setup() {
  // put your setup code here, to run once:
  //button adc input
  pinMode( BUTTON_ADC_PIN, INPUT );         //ensure A0 is an input
  digitalWrite( BUTTON_ADC_PIN, LOW );      //ensure pullup is off on A0
  //lcd backlight control
  digitalWrite( LCD_BACKLIGHT_PIN, HIGH );  //backlight control pin D3 is high (on)
  pinMode( LCD_BACKLIGHT_PIN, OUTPUT );     //D3 is an output
  //set up the LCD number of columns and rows:
  lcd.begin( 16, 2 );
}

/*--------------------------------------------------------------------------------------
  loop()
  Arduino main loop
  --------------------------------------------------------------------------------------*/
void loop() {
  // put your main code here, to run repeatedly:
  lcd.setCursor( 3, 0 );
  lcd.print( "BLACKJACK" );
  lcd.setCursor( 0, 1 );
  lcd.print( "*Play   How to" );

  byte menu_selected = OPTION_PLAY;
  byte read_input = ReadButtons();
  while (BUTTON_SELECT != read_input)
  {
    switch (read_input)
    {
      case BUTTON_RIGHT:
      case BUTTON_LEFT:
      case BUTTON_UP:
      case BUTTON_DOWN:
        if (OPTION_PLAY == menu_selected)
        {
          lcd.setCursor( 0, 1 );
          lcd.print(" ");
          lcd.setCursor( 7, 1 );
          lcd.print("*");
          menu_selected = OPTION_HOW_TO;
        } else
        {
          lcd.setCursor( 7, 1 );
          lcd.print(" ");
          lcd.setCursor( 0, 1 );
          lcd.print("*");
          menu_selected = OPTION_PLAY;
        }
        break;
      case BUTTON_NONE:
      default:
        break;
    }
    read_input = ReadButtons();
  }

  if (OPTION_PLAY == menu_selected)
  {
    blackjack();
  }
  else if (OPTION_HOW_TO == menu_selected)
  {
    how_to();
  }
  else
  {
    //ERROR, SHOULD NOT REACH
    lcd.setCursor( 0, 0 );
    lcd.print( "ERROR ODD SELECT" );
    while (1);
  }
}


/*--------------------------------------------------------------------------------------
  blackjack()
  The main Blackjack game function.
  Will only exit once user completes game and selects to not play again.

  --------------------------------------------------------------------------------------*/
void blackjack() {
  lcd.setCursor( 0, 0 );
  lcd.print( "Blackjack game" );
  while (BUTTON_SELECT != ReadButtons());
}


/*--------------------------------------------------------------------------------------
  how_to()
  A short instructional on how to play

  --------------------------------------------------------------------------------------*/
void how_to() {
  lcd.setCursor( 0, 0 );
  lcd.print( "How to play" );
  while (BUTTON_SELECT != ReadButtons());
}


/*--------------------------------------------------------------------------------------
  ReadButtons()
  Detect the button pressed and return the value
  Uses global values buttonWas, buttonJustPressed, buttonJustReleased.

  --------------------------------------------------------------------------------------*/
byte ReadButtons()
{
  unsigned int buttonVoltage;
  byte button = BUTTON_NONE;   // return no button pressed if the below checks don't write to btn
  static byte buttonWas          = BUTTON_NONE;   //used by ReadButtons() for detection of button events

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
  // Only return the pressed button if it was a rising edge
  if ( buttonWas == BUTTON_NONE )
  {
    buttonWas = button;
    return ( button );
  }

  // save the latest button value, for change event detection next time round
  buttonWas = button;
  // Return no button pressed if not a rising edge
  return ( BUTTON_NONE );
}
