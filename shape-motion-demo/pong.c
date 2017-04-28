#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>

#define GREEN_LED BIT6;

AbRect left_paddle = {abRectGetBounds, abRectCheck,{5,18}};
AbRect right_paddle = {abRectGetBounds, abRectCheck,{5,18}};

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 10, screenHeight/2 - 10}
};

Layer layer0 = {	//contains the ball      
  (AbShape *)&circle8,
  {(screenWidth/2)+10, (screenHeight/2)+5},
  {0,0}, {0,0},			  
  COLOR_BLUE,
  0
};

Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_GREEN,
  &layer0
};
 
Layer layer1 = { //contains left paddle
    (AbShape *)&left_paddle,
    {(screenWidth/2)-48, (screenHeight/2) - 51},
    {0,0}, {0,0},		    
    COLOR_WHITE,
    &fieldLayer,
};

Layer layer2 = { //contains right paddle
  (AbShape *)&right_paddle,
  {(screenWidth/2)+48, (screenHeight/2) + 51},
  {0,0}, {0,0},		    
  COLOR_WHITE,
  &layer1,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer ml0 = { &layer0, {3,3}, 0 };
MovLayer ml1 = { &layer1, {0,0}, &ml0 };
MovLayer ml2 = { &layer2, {0,0}, &ml1 };

void movLayerDraw(MovLayer *movLayers, Layer *layers){
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  

//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */

int collision_detector(MovLayer *moving_layer, Vec2 *newPos, u_int axis){

  int velocity2 = 0;

  //if the ball is touching any of the paddles
  if(abShapeCheck(ml1.layer->abShape,&ml1.layer->posNext, newPos) ||
     (abShapeCheck(ml2.layer->abShape,&ml2.layer->posNext, newPos))){

    //change the volocity of the ball to its opposite
    velocity2 = ml0.velocity.axes[axis] = -ml0.velocity.axes[axis];
    return velocity2;

  }
}

void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  int check_score;
  
  for (; ml; ml = ml->next) {

    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);

    for (axis = 0; axis < 2; axis ++) {

      //checks collision between figures and fence
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {

	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
      }
      
      /*Checks for collision between the ball and the paddles, in order to do so
	it checks if the ball shape equals to the actual moving layer, so it can collide */
      if (ml->layer->abShape == ml0.layer->abShape){
	int velocity = collision_detector(ml,&newPos,axis);
	newPos.axes[axis] += (2*velocity);
      }
     
    ml->layer->posNext = newPos; //advance to the next layer
    }
  }
}


u_int bgColor = COLOR_BLACK;
int redrawScreen =1;
Region fieldFence;

/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main()
{  
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);
  shapeInit();
  layerInit(&layer2);
  layerDraw(&layer2);
  layerGetBounds(&fieldLayer, &fieldFence);
  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */

  for(;;) { 
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml2, &layer2);
    //Placing the score
    drawString5x7(0,0, "P1 SCORE: ", COLOR_RED, COLOR_BLACK);
    drawString5x7(72,152,":SCORE P2 ", COLOR_RED, COLOR_BLACK);
    
  }
}

/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  //int switches = ~p2sw_read();
  if (count == 15) {
    mlAdvance(&ml2, &fieldFence);
  /*LEFT PADDLE CONTROLLER */

    int switches = ~p2sw_read();

    if (switches & BIT0){ //going up
      ml1.velocity.axes[1] = 2;
      // layer1.posNext.axes[1] -= 1;
      //layer1.pos.axes[1] -= 1;
      //layer1.posLast.axes[1] -=1;
      // redrawScreen = 1;
      //count = 0;
    }
    
    else if (switches & BIT1){ //going down
      ml1.velocity.axes[1] = -2;
      // layer1.posNext.axes[1] += 1;
      //layer1.pos.axes[1] -= 1;
      //layer1.posLast.axes[1] -=1;
      // redrawScreen = 1;
      //count = 0;
    }
  /* RIGHT PADDLE CONTROLLER */
    
    else if (switches & BIT2){ //going down
      ml2.velocity.axes[1] = 2;
      //layer1.pos.axes[1] -= 1;
      //layer1.posLast.axes[1] -=1;
      // redrawScreen = 1;
      //count = 0;
    }
    else if (switches & BIT3){ //going down
      ml2.velocity.axes[1] = -2;
      // layer2.posNext.axes[1] -= 1;
      //layer1.pos.axes[1] -= 1;
      //layer1.posLast.axes[1] -=1;
      //redrawScreen = 1;
      //count = 0;
	 }

    else{
      ml1.velocity.axes[1] = 0;
      ml2.velocity.axes[1] = 0;
    }
    redrawScreen =1; // False
    count =0;
  }
  
  
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
