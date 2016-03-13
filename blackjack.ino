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
#define OPTION_ONE             1
#define OPTION_TWO             2
// Mask for debouncing 0b00111111
#define DEBOUNCE_BIT_MASK 0x3f
// For draw_new_cards function
#define NEW_HAND              1
#define NEW_CARD              2
#define BLACKJACK_SCORE       50
#define FIVE_UNDER_SCORE      40
#define BUST_SCORE            30
/*--------------------------------------------------------------------------------------
  Variables
  --------------------------------------------------------------------------------------*/

const char * const cards_strings[] = { "F", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

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
  // Initialise random seed
  /*
  pinMode( A2, INPUT);
  digitalWrite( BUTTON_ADC_PIN, LOW );
  randomSeed(analogRead(A2));
  pinMode( A2, OUTPUT);
  */
}

/*--------------------------------------------------------------------------------------
  loop()
  Arduino main loop
  --------------------------------------------------------------------------------------*/
void loop() {
  // put your main code here, to run repeatedly:
  clear_lcd();
  lcd.setCursor( 3, 0 );
  lcd.print( "BLACKJACK" );
  lcd.setCursor( 0, 1 );
  lcd.print( "*Play   How to" );

  // OPTION_ONE is "Play", OPTION_TWO is "How to"
  byte menu_selected = query_user_input( 0, 1, 7, 1 );

  if (OPTION_ONE == menu_selected)
  {
    blackjack();
  }
  else if (OPTION_TWO == menu_selected)
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
  do
  {
    byte player_total = 1;
    byte dealer_total = 1;  // Initialise to 1 so if player busts and their score set to 0 for comparison, dealer still wins
    // Player turn
    clear_lcd();
    lcd.setCursor( 0, 0 );
    lcd.print( "Player:" );
    player_total = draw_new_cards(NEW_HAND);
    while (21 >= player_total)
    {
      lcd.setCursor( 7, 0 );
      lcd.print( " *Hit Sit" );
      if( OPTION_ONE == query_user_input( 8, 0, 12, 0 ) )
      {
        //Hit
        player_total = draw_new_cards(NEW_CARD);
      }
      else
      {
        break;
      }
    }
    
    if (BUST_SCORE != player_total)   // If player busted, skip to winner calculation
    {
      // Dealer turn
      clear_lcd();
      lcd.setCursor( 0, 0 );
      lcd.print( "Dealer:" );
      dealer_total = draw_new_cards(NEW_HAND);
      while (21 >= dealer_total)
      {
        lcd.setCursor( 7, 0 );
        lcd.print( "         " );
        if( 17 > dealer_total )
        {
          //Hit
          lcd.setCursor( 7, 0 );
          lcd.print( "      Hit" );
          while (BUTTON_SELECT != ReadButtons());
          dealer_total = draw_new_cards(NEW_CARD);
        }
        else
        {
          lcd.setCursor( 7, 0 );
          lcd.print( "      Sit" );
          while (BUTTON_SELECT != ReadButtons());
          break;
        }
      }
    }
    // Set scores to zero if busted so a simple greater than comparison can be used
    if (BUST_SCORE == player_total)
    {
      player_total = 0;
    }
    if (BUST_SCORE == dealer_total)
    {
      dealer_total = 0;
    }
    
    clear_lcd();
    if ( player_total > dealer_total)
    {
      lcd.setCursor( 0, 0 );
      lcd.print("You win!");
    }
    else if ( dealer_total > player_total)
    {
      lcd.setCursor( 0, 0 );
      lcd.print("You lose");
    }
    else
    {
      lcd.setCursor( 0, 0 );
      lcd.print("Draw");
    }
    
    lcd.setCursor( 13, 0 );
    lcd.print("Yes");
    lcd.setCursor( 0, 1 );
    lcd.print("Play again?   No" );
  } while (OPTION_TWO != query_user_input( 12, 0, 13, 1));

}


/*--------------------------------------------------------------------------------------
  draw_new_cards()
    Descriptions:
      
    Inputs:
      Indicator of whether cards are being drawn to a new hand or to an existing hand
    Outputs:
      
  --------------------------------------------------------------------------------------*/
byte draw_new_cards( const byte new_hand_or_card )
{
  static byte hand[5] = {0};
  static byte number_of_cards = 0;
  static byte cursor_pointer = 15;           // End of screen
  byte total = 0;
  byte ace_count = 0;

  if(NEW_HAND == new_hand_or_card)
  {
    memset( hand, 0, 5);
    number_of_cards = 0;
    cursor_pointer = 15;
    draw_and_print( &(hand[number_of_cards++]), &cursor_pointer );
  }
  draw_and_print( &(hand[number_of_cards++]), &cursor_pointer );

  for ( byte i = 0; i < number_of_cards; i++)
  {
    if (10 < hand[i] )      // Picture card
    {
      total += 10;
    }
    else if ( 1 < hand[i])  // 2-10 card
    {
      total += hand[i];
    }
    else                    // Ace
    {
      total += 11;
      ace_count++;
    }
  }
  // Print current total to bottom left corner
  lcd.setCursor( 0, 1);
  lcd.print(total);
  // Check if Blackjack or if Ace acting as 11 should act as 1
  if (0 < ace_count)
  {
    while ( ( 21 < total ) && (0 < ace_count))
    {
      total -= 10;
      ace_count --;
    }
    // Update after changing Ace value
    lcd.setCursor( 0, 1);
    lcd.print(total);
    
    if ((2 == number_of_cards) && (21 == total))
    {
      // Blackjack
      lcd.setCursor( 7, 0 );
      lcd.print("Blackjack");
      while (BUTTON_SELECT != ReadButtons());
      return BLACKJACK_SCORE;
    }
  }

  if (21 < total)
  {
    // Bust
    lcd.setCursor( 7, 0 );
    lcd.print("     Bust");
    while (BUTTON_SELECT != ReadButtons());
    return BUST_SCORE;
  }
  if (5 <= number_of_cards)       // To reach this point, total must be below 21        
  {
    // 5 under
    lcd.setCursor( 7, 0 );
    lcd.print("  5 Under");
    while (BUTTON_SELECT != ReadButtons());
    return FIVE_UNDER_SCORE;
  }
  return total;
  
}


/*--------------------------------------------------------------------------------------
  draw_new_cards()
    Descriptions:
      Creates a random card and places it in the given location. Keeps track of cursor pointer and prints
      to the appropriate location on screen.
    Inputs:
      Pointer to card location.
      Pointer to cursor location.
    Outputs:
      Updates cursor location
      Places new card in card location
      Prints card to screen
  --------------------------------------------------------------------------------------*/
void draw_and_print( byte *card, byte *cursor_pointer )
{
  *card = random(1,14);
  if (10 == *card)
  {
    (*cursor_pointer)--;
  }
  lcd.setCursor( *cursor_pointer, 1);
  lcd.print( cards_strings[*card]);
  (*cursor_pointer)--;
  (*cursor_pointer)--;
}




/*--------------------------------------------------------------------------------------
  how_to()
  A short instructional on how to play

  --------------------------------------------------------------------------------------*/
void how_to() {
  clear_lcd();
  lcd.setCursor( 0, 0 );
  lcd.print( "How to play" );
  while (BUTTON_SELECT != ReadButtons());
}



void clear_lcd() {
  lcd.setCursor( 0, 0 );
  lcd.print( "                " );
  lcd.setCursor( 0, 1 );
  lcd.print( "                " );
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

  // Debouncing. Shift bits until 7 straight detections after
  debounce_detection << 1;
  if (buttonWas != button)
  {
    debounce_detection++;
    delay(1);
  }

  if ( DEBOUNCE_BIT_MASK == debounce_detection)
  {
    debounce_detection = 0;
    buttonWas = button;
    return ( button );
  }
  // If not a rising edge debounce
  return ( BUTTON_NONE );
}


/*--------------------------------------------------------------------------------------
  query_user_input()
    Descriptions:
      Moves the cursor between two given locations and waits until the user selects one.
      Can be used for any two cursor locations provided the code has already printed the options to screen.
    Inputs:
      X and Y locations for two points to place an asterisk indicating the selected option.
    Outputs:
      Prints asterisk to screen, other functions required to clear asterisk.
      Returns either the constant OPTION_ONE or OPTION_TWO depending on user choice.
  --------------------------------------------------------------------------------------*/
byte query_user_input(byte one_x, byte one_y, byte two_x, byte two_y)
{
  byte button_pressed = BUTTON_NONE;
  byte option_selected = OPTION_ONE;
  // Set default cursor position
  lcd.setCursor( one_x, one_y );
  lcd.print("*");
  lcd.setCursor( two_x, two_y );
  lcd.print(" ");

  do
  {
    button_pressed = ReadButtons();
    switch (button_pressed)
    {
      case BUTTON_RIGHT:
      case BUTTON_LEFT:
      case BUTTON_UP:
      case BUTTON_DOWN:
        if (OPTION_ONE == option_selected)
        {
          // Change from option one to option two
          lcd.setCursor( one_x, one_y );
          lcd.print(" ");
          lcd.setCursor( two_x, two_y );
          lcd.print("*");
          option_selected = OPTION_TWO;
        } else
        {
          // Change from option two to option one
          lcd.setCursor( two_x, two_y );
          lcd.print(" ");
          lcd.setCursor( one_x, one_y );
          lcd.print("*");
          option_selected = OPTION_ONE;
        }
        break;
      case BUTTON_NONE:
      default:
        break;
    }
  } while (BUTTON_SELECT != button_pressed);
  return option_selected;
}

