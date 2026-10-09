/*
NVHDisplay.c
File contains the NVHDisplay functions for a esp32c6.

Written by Cole Perera & Ridwaan Joomun for Sheffield Formula Racing 2025
*/

#include "NVHDisplay.h"

/* --------------------------- Definitions ---------------------------------- */
#define MEM_LOGO 0x000f8000
#define MEM_PIC1 0x000fa000
#define PROGMEM
#define LAYOUT_Y1 40 // size of rectangle at top

#define RED     0xff0000UL
#define ORANGE  0xffa500UL
#define GREEN   0x00ff00UL
#define BLUE    0x0000ffUL
#define BLUE_1  0x5dade2L
#define YELLOW  0xffff00UL
#define MAGENTA 0xff00ffUL
#define PINK 0xFF2C7D
#define PURPLE  0x800080UL
#define WHITE   0xffffffUL
#define BLACK   0x000000UL
#define DARK_GREY 0x404040UL
#define YELLOW2  0x00FFCC00
#define LIGHT_GREY 0x00999999
#define DARK_GREEN 0x00009900

#define MEM_DL_STATIC (EVE_RAM_G_SIZE - 4096)
#define CELL_TEMP_WARNING_THRESHOLD 45  // degrees C - adjust to whatever the team decides

#define POLE_PAIRS          10      // motor pole pairs
#define GEAR_RATIO          3.0f    // update if incorrect gear ratio
#define WHEEL_RADIUS_M      0.2f  // update if incorrect wheel radius (in meters)
#define MPH_SPEED_GAIN      1.0f    // tuning gain - adjust if reading doesn't match

/* --------------------------- Global Variables ----------------------------- */

/* --------------------------- Local Variables ------------------------------ */
byte byTFTActive = 0;
dword dwNStaticDisplaySize = 0;
word wToggleState = 0;
word wDisplayListSize = 0;
word wNProfileA = 0;
word wNProfileB = 0;
uint32_t colour = GREEN;
const char* message = "";

/* --------------------------- Screen Variables ------------------------------ */
char BatteryBuffer[5];   // Buffer to hold the battery SOC string
char CellTempBuffer[5];   // Buffer to hold the cell temperature string
char MotorTempBuffer[5];   // Buffer to hold the motor temperature string
char InverterTempBuffer[5];   // Buffer to hold the inverter temperature string
char SpeedBuffer[5];   // Buffer to hold the speed string

/* --------------------------- Function prototypes -------------------------- */

/* --------------------------- Functions ------------------------------------ */
void NVHDisplay_init(void)
{
    /*
    *===========================================================================
    *   NVHDisplay_init
    *   Takes:   None
    * 
    *   Returns: None
    * 
    *   Initializes the NVH Display interface.
    *===========================================================================
    *   Revision History:
    *   14/11/25 CP Initial Version
    *
    *===========================================================================
    */

    /* Initialize EVE SPI interface */
    EVE_init_spi();

    if(E_OK == EVE_init())  /*Does EVE powerup and if init successful*/
    {
        byTFTActive = TRUE;

        EVE_memWrite8(REG_PWM_DUTY, 0x50);  /* setup backlight, range is from 0 = off to 0x80 = max */
        EVE_calibrate_write(0x0000f78b, 0x00000427, 0xfffcedf8, 
            0xfffffba4, 0x0000f756, 0x0009279e);

        EVE_cmd_setrotate(3);   // good orientation for vertical

        initStaticBackground();
    }
}

void initStaticBackground(void)
{
    EVE_cmd_dlstart(); /* Start the display list */
    EVE_tag(0); /* tag = 0 - do not use the following objects for touch-detection */

    EVE_cmd_bgcolor(0x000000UL); /* light grey */
    EVE_vertex_format(0); /* set to 0 - reduce precision for VERTEX2F to 1 pixel instead of 1/16 pixel default */

    /* draw a rectangle on top */
    EVE_begin(EVE_RECTS);
    EVE_line_width(1U*16U); /* size is in 1/16 pixel */
    EVE_color_rgb(DARK_GREY);
    EVE_vertex2f(0, 0); /* set to 0 / 0 */
    EVE_vertex2f(EVE_HSIZE,LAYOUT_Y1-2);
    EVE_end();

    /* display the logo */
    // EVE_color_rgb(WHITE);
    // EVE_begin(EVE_BITMAPS);
    // EVE_cmd_setbitmap(MEM_LOGO, EVE_ARGB1555, 56U, 56U);
    // EVE_vertex2f(EVE_HSIZE - 58, 5);
    // EVE_end();

    /* draw a black line to separate things */
    // EVE_color_rgb(PINK);
    // EVE_begin(EVE_LINES);
    // EVE_vertex2f(EVE_HSIZE-35,0);
    // EVE_vertex2f(EVE_HSIZE-35, EVE_HSIZE-35);
    // EVE_end();
    
    EVE_color_rgb(WHITE);
    EVE_cmd_text(EVE_VSIZE/2-15, 5, 29, EVE_OPT_CENTERX, "S");
    EVE_color_rgb(YELLOW2);
    EVE_cmd_text(EVE_VSIZE/2, 5, 29, EVE_OPT_CENTERX, "F");
    EVE_color_rgb(WHITE);
    EVE_cmd_text(EVE_VSIZE/2+15, 5, 29, EVE_OPT_CENTERX, "R");

    char abySOCBuffer[4];
    sprintf(abySOCBuffer, "%3d", (uint8_t)(rSOC));  // convert battery percentage to string for display

    // // Rectangle for battery percentage
    EVE_widget_rectangle(EVE_VSIZE/2 - 80, 50 , 160U, 80U, 0, 10, WHITE);  // x, y, width, height, border, transparency, colour
    EVE_color_rgb(DARK_GREY);
    EVE_cmd_text(EVE_VSIZE/2 - 75, 55, 20, 0, "Battery:");  //x, y, font
    EVE_color_rgb(WHITE);

    // Rectangle Cell temperature
    EVE_color_rgb(WHITE);
    EVE_widget_rectangle(10, EVE_HSIZE/2 -15, 100U, 50U, 0, 4, ORANGE);  // x, y, width, height, border, transparency, colour
    EVE_color_rgb(WHITE);
    EVE_cmd_text(12, EVE_HSIZE/2 -13, 20, 0, "Cell temp:");  //x, y, font
    EVE_color_rgb(WHITE);

    // // Rectangle Motor temperature
    EVE_widget_rectangle(EVE_VSIZE/2+10, EVE_HSIZE/2 -15, 100U, 50U, 0, 4, ORANGE);  // x, y, width, height, border, transparency, colour
    EVE_color_rgb(WHITE);
    EVE_cmd_text(EVE_VSIZE/2+12, EVE_HSIZE/2 -13, 20, 0, "Motor temp:");  //x, y, font


    // // Rectangle Inverter temperature
    EVE_widget_rectangle(10, EVE_HSIZE/2 +45, 100U, 50U, 0, 4, ORANGE);  // x, y, width, height, border, transparency, colour
    EVE_color_rgb(WHITE);
    EVE_cmd_text(12, EVE_HSIZE/2 +48, 20, 0, "Inverter temp:");  //x, y, font


    // // Rectangle Coolant temperature
    EVE_widget_rectangle(EVE_VSIZE/2+10, EVE_HSIZE/2 +45, 100U, 50U, 0, 4, ORANGE);  // x, y, width, height, border, transparency, colour
    EVE_color_rgb(WHITE);
    EVE_cmd_text(EVE_VSIZE/2+12, EVE_HSIZE/2 +48, 20, 0, "Speed:");  //x, y, font


    // Message rectangle
    EVE_widget_rectangle(EVE_VSIZE/2 - 110, EVE_HSIZE-50, 220U, 35U, 0, 10, WHITE);  // x, y, width, height, border, transparency, colour
    EVE_color_rgb(BLACK);
    EVE_cmd_text(EVE_VSIZE/2 - 105, EVE_HSIZE-40, 20, 0, "Messages:");  //x, y, font



    EVE_execute_cmd();

    dwNStaticDisplaySize = EVE_memRead16(REG_CMD_DL);

    EVE_cmd_memcpy(MEM_DL_STATIC, EVE_RAM_DL, dwNStaticDisplaySize);
    EVE_execute_cmd();
}

void TFT_display(void)
{
    // static int32_t rotate = 0;

    if(byTFTActive != 0U)
    {
        EVE_start_cmd_burst(); /* start writing to the cmd-fifo as one stream of bytes, only sending the address once */
        EVE_cmd_dlstart(); /* start the display list */
        EVE_clear_color_rgb(BLACK); /* set the default clear color to white */
        EVE_clear(1, 1, 1); /* clear the screen - this and the previous prevent artifacts between lists, Attributes are the color, stencil and tag buffers */
        EVE_tag(0); /* no touch */
        EVE_cmd_append(MEM_DL_STATIC, dwNStaticDisplaySize); /* insert static part of display-list from copy in gfx-mem */

        if (rSOC < 0.01f && TCellAvg == 0 && Actual_TempMotor < 0.01f && Actual_TempController < 0.01f) {
            colour = ORANGE;
            message = "No CAN data!";
        } 
        // Danger messages
        // Low Battery
        else if (rSOC < 20.0f) {
            colour = RED;
            message = "Low battery!";
        }
        // High cell temperature
        else if (TCellAvg > CELL_TEMP_WARNING_THRESHOLD) {
            colour = RED;
            message = "High cell temp!";
        }

        else {
            colour = GREEN;
            message = " ";
        }

        // Status Circles
        EVE_widget_circle(20, 18, 150, 1, colour);
        EVE_widget_circle(48, 18, 150, 1, colour);
        EVE_widget_circle(76, 18, 150, 1, colour);
        EVE_widget_circle(EVE_VSIZE-20, 18, 150, 1, colour);
        EVE_widget_circle(EVE_VSIZE-48, 18, 150, 1, colour);
        EVE_widget_circle(EVE_VSIZE-76, 18, 150, 1, colour);


        // battery percentage
        sprintf(BatteryBuffer, "%3.0f%%", rSOC);  
        EVE_color_rgb(BLACK);
        EVE_cmd_text(EVE_VSIZE/2-5, EVE_HSIZE/2-65, 31, EVE_OPT_CENTER, BatteryBuffer);  //x, y, font

        // Cell temperature
        sprintf(CellTempBuffer, "%3d", TCellAvg);  
        EVE_color_rgb(WHITE);
        EVE_cmd_text(50, EVE_HSIZE/2 +18, 30, EVE_OPT_CENTER, CellTempBuffer);  //x, y, font

        // Motor temperature
        sprintf(MotorTempBuffer, "%3.0f", Actual_TempMotor);  
        EVE_color_rgb(WHITE);
        EVE_cmd_text(EVE_VSIZE/2+50, EVE_HSIZE/2 +18, 30, EVE_OPT_CENTER, MotorTempBuffer);  //x, y, font

        // Inverter temperature
        sprintf(InverterTempBuffer, "%3.0f", Actual_TempController);  
        EVE_color_rgb(WHITE);
        EVE_cmd_text(50, EVE_HSIZE/2 +78, 30, EVE_OPT_CENTER, InverterTempBuffer);  //x, y, font

        // Speed
        float fMotorRPM = Actual_ERPM / POLE_PAIRS;
        float fWheelRPM = fMotorRPM / GEAR_RATIO;
        float fSpeedMPH = (fWheelRPM * 2.0f * 3.14159f * WHEEL_RADIUS_M / 60.0f) * 2.237f * MPH_SPEED_GAIN;
        sprintf(SpeedBuffer, "%3.0f", fSpeedMPH);
        EVE_color_rgb(WHITE);
        EVE_cmd_text(EVE_VSIZE/2+50, EVE_HSIZE/2 +78, 30, EVE_OPT_CENTER, SpeedBuffer);  //x, y, font

        // Messages
        EVE_color_rgb(RED);
        EVE_cmd_text(EVE_VSIZE/2 - 50, EVE_HSIZE-45, 28, 0, message);  //x, y, font

        // Danger warning box
        if (colour == RED) {
            EVE_color_rgb(YELLOW);
            EVE_widget_rectangle(EVE_VSIZE/2 - 80, EVE_HSIZE/2 - 30, 160U, 60U, 3, 10, YELLOW);  // yellow border
            EVE_widget_rectangle(EVE_VSIZE/2 - 77, EVE_HSIZE/2 - 27, 154U, 54U, 0, 15, RED);     // red fill
            EVE_color_rgb(BLACK);
            EVE_cmd_text(EVE_VSIZE/2, EVE_HSIZE/2, 30, EVE_OPT_CENTER, "DANGER");

            // Left triangle
            EVE_color_rgb(ORANGE);
            EVE_begin(EVE_LINE_STRIP);
            EVE_vertex2f(EVE_VSIZE/2 - 115, 110);   // bottom left
            EVE_vertex2f(EVE_VSIZE/2 - 95, 75);     // top middle
            EVE_vertex2f(EVE_VSIZE/2 - 75, 110);    // bottom right
            EVE_vertex2f(EVE_VSIZE/2 - 115, 110);   // close the triangle
            EVE_end();
            EVE_color_rgb(WHITE);
            EVE_cmd_text(EVE_VSIZE/2 - 95, 93, 29, EVE_OPT_CENTER, "!");

            // Right triangle
            EVE_color_rgb(ORANGE);
            EVE_begin(EVE_LINE_STRIP);
            EVE_vertex2f(EVE_VSIZE/2 + 75, 110);    // bottom left
            EVE_vertex2f(EVE_VSIZE/2 + 95, 75);     // top middle
            EVE_vertex2f(EVE_VSIZE/2 + 115, 110);   // bottom right
            EVE_vertex2f(EVE_VSIZE/2 + 75, 110);    // close the triangle
            EVE_end();
            EVE_color_rgb(WHITE);
            EVE_cmd_text(EVE_VSIZE/2 + 95, 93, 29, EVE_OPT_CENTER, "!");
        }

        EVE_restore_context();
        EVE_display(); /* mark the end of the display list */
        EVE_cmd_swap(); /* make this list active */

        EVE_end_cmd_burst(); /* stop writing to the cmd-fifo, the cmd-FIFO will be executed automatically after this or when DMA is done */
    }
}

esp_err_t display_empty_buffer(void)
{
    esp_err_t NStatus = ESP_OK;
    CAN_frame_t stCANFrame; 
    
    if (!xCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (uxQueueMessagesWaiting(xCANRingBuffer) == 0)
    {
        /* Buffer empty */
        return ESP_OK;
    }

    while (xQueueReceive(xCANRingBuffer, &stCANFrame, 0) == pdTRUE)
    {
        /* Look for relevent CAN frames */
        switch (stCANFrame.dwID)
        {
            // battery percentage
            case CELLSTATS1_ID:
                CellStats1Rx(stCANFrame);   // writes to Pack_SOC globally for battery PERCENTAGE
                break;
            
            // BMS Cell Temperature
            case BMSCELLTEMP_ID:
                BMSCellTempRx(stCANFrame);   // writes to TCellAvg globally
                break;
            
            // Motor & Inverter temperature
            case TEMPERATURES_ID:
                TemperaturesRx(stCANFrame); // writes to Actual_TempMotor & Actual_TempController (respectively) globally
                break;

            // ERON speed
            case ERPM_DUTY_VOLTAGE_ID:
                ERPM_DUTY_VOLTAGERx(stCANFrame); // writes ERPM globally for speed calculation
                break;

            default:
                /* Ignore other CAN frames */
                break;
        }
    }

    return NStatus;
}
