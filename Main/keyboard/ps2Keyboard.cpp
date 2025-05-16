#include <ctype.h>
#include <keyboard/ps2Keyboard.h>
#include <keyboard/queue.h>

#include "stm32h7xx_hal.h"

typedef enum
{
    IDLE = 10,
    BIT0 = 0,
    BIT1 = 1,
    BIT2 = 2,
    BIT3 = 3,
    BIT4 = 4,
    BIT5 = 5,
    BIT6 = 6,
    BIT7 = 7,
    PARITY = 8,
    STOP = 9
} ps2_read_status;

static int32_t ps2_status;
static int32_t kb_data;

static uint8_t lastClk = 1;
static uint8_t lastData;
static bool _isLeftShiftPressed;
static bool _isRightShiftPressed;
static volatile uint8_t _parity;

void Ps2_Initialize()
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct;

    // CLK pin
    GPIO_InitStruct.Pin = CLK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // DATA pin
    GPIO_InitStruct.Pin = DATA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    ps2_status = IDLE;
    QueueInit();
    kb_data = 0;
    _isLeftShiftPressed = false;
    _isRightShiftPressed = false;
}

int32_t Ps2_GetScancode()
{
    int32_t result;
    if (QueueGet(&result) == -1)
    {
        result = -1;
    }

    int32_t code = result & 0xFF;
    if (code == KEY_LEFTSHIFT)
    {
    	_isLeftShiftPressed = (result & 0xFF00) != 0xF000;
    }
    if (code == KEY_RIGHTSHIFT)
    {
    	_isRightShiftPressed = (result & 0xFF00) != 0xF000;
    }

    return result;
}

char Ps2_ConvertScancode(int32_t scanCode)
{
	char result;

	switch (scanCode)
	{
	case KEY_A:
		result = 'a';
		break;
	case KEY_B:
		result = 'b';
		break;
	case KEY_C:
		result = 'c';
		break;
	case KEY_D:
		result = 'd';
		break;
	case KEY_E:
		result = 'e';
		break;
	case KEY_F:
		result = 'f';
		break;
	case KEY_G:
		result = 'g';
		break;
	case KEY_H:
		result = 'h';
		break;
	case KEY_I:
		result = 'i';
		break;
	case KEY_J:
		result = 'j';
		break;
	case KEY_K:
		result = 'k';
		break;
	case KEY_L:
		result = 'l';
		break;
	case KEY_M:
		result = 'm';
		break;
	case KEY_N:
		result = 'n';
		break;
	case KEY_O:
		result = 'o';
		break;
	case KEY_P:
		result = 'p';
		break;
	case KEY_Q:
		result = 'q';
		break;
	case KEY_R:
		result = 'r';
		break;
	case KEY_S:
		result = 's';
		break;
	case KEY_T:
		result = 't';
		break;
	case KEY_U:
		result = 'u';
		break;
	case KEY_V:
		result = 'v';
		break;
	case KEY_W:
		result = 'w';
		break;
	case KEY_X:
		result = 'x';
		break;
	case KEY_Y:
		result = 'y';
		break;
	case KEY_Z:
		result = 'z';
		break;
	case KEY_0_CPARENTHESIS:
		result = '0';
		break;
	case KEY_1_EXCLAMATION_MARK:
		result = '1';
		break;
	case KEY_2_AT:
		result = '2';
		break;
	case KEY_3_NUMBER_SIGN:
		result = '3';
		break;
	case KEY_4_DOLLAR:
		result = '4';
		break;
	case KEY_5_PERCENT:
		result = '5';
		break;
	case KEY_6_CARET:
		result = '6';
		break;
	case KEY_7_AMPERSAND:
		result = '7';
		break;
	case KEY_8_ASTERISK:
		result = '8';
		break;
	case KEY_9_OPARENTHESIS:
		result = '9';
		break;
	case KEY_BACKSPACE:
		result = '\b';
		break;
	case KEY_SPACEBAR:
		result = ' ';
		break;
	case KEY_COMMA_AND_LESS:
		result = ',';
		break;
	case KEY_MINUS_UNDERSCORE:
		result = '-';
		break;
	case KEY_DOT_GREATER:
		result = '.';
		break;
	case KEY_SLASH_QUESTION:
		result = '/';
		break;
	case KEY_SINGLE:
		result = '`';
		break;
	case KEY_APOS:
		result = '\'';
		break;
	case KEY_SEMICOLON_COLON:
		result = ';';
		break;
	case KEY_BACK:
		result = '\\';
		break;
	case KEY_OPEN_SQ:
		result = '[';
		break;
	case KEY_CLOSE_SQ:
		result = ']';
		break;
	case KEY_EQUAL_PLUS:
		result = '=';
		break;
	default:
		result = '\0';
		break;
	}

	if (_isLeftShiftPressed || _isRightShiftPressed)
	{
		switch (scanCode)
		{
		case KEY_0_CPARENTHESIS:
			result = ')';
			break;
		case KEY_1_EXCLAMATION_MARK:
			result = '!';
			break;
		case KEY_2_AT:
			result = '@';
			break;
		case KEY_3_NUMBER_SIGN:
			result = '#';
			break;
		case KEY_4_DOLLAR:
			result = '$';
			break;
		case KEY_5_PERCENT:
			result = '%';
			break;
		case KEY_6_CARET:
			result = '^';
			break;
		case KEY_7_AMPERSAND:
			result = '&';
			break;
		case KEY_8_ASTERISK:
			result = '*';
			break;
		case KEY_9_OPARENTHESIS:
			result = '(';
			break;
		case KEY_COMMA_AND_LESS:
			result = '<';
			break;
		case KEY_MINUS_UNDERSCORE:
			result = '_';
			break;
		case KEY_DOT_GREATER:
			result = '>';
			break;
		case KEY_SLASH_QUESTION:
			result = '?';
			break;
		case KEY_SINGLE:
			result = '~';
			break;
		case KEY_APOS:
			result = '"';
			break;
		case KEY_SEMICOLON_COLON:
			result = ':';
			break;
		case KEY_BACK:
			result = '|';
			break;
		case KEY_OPEN_SQ:
			result = '{';
			break;
		case KEY_CLOSE_SQ:
			result = '}';
			break;
		case KEY_EQUAL_PLUS:
			result = '+';
			break;
		default:
			result = toupper(result);
			break;
		}
	}

	return result;
}

inline void Update(uint8_t dataBit)
{
    switch (ps2_status)
    {
    case IDLE:
        ps2_status = BIT0;
        _parity = 0;
        break;

    case BIT0:
        ps2_status = BIT1;
        kb_data |= dataBit;
        _parity ^= dataBit;
        break;

    case BIT1:
    case BIT2:
    case BIT3:
    case BIT4:
    case BIT5:
    case BIT6:
    case BIT7:
        kb_data |= (dataBit << ps2_status);
        ps2_status++;
        _parity ^= dataBit;
        break;

    case PARITY:
        if (_parity == dataBit) 
        {
            // Parity error
            _parity = 0xFD;
            kb_data = 0;

            // TODO ask to resend
        }
        ps2_status = STOP;
        break;

    case STOP:
        ps2_status = IDLE;
        if (_parity != 0xFD)
        {
            if ((kb_data & 0xE0) == 0xE0)
            {
                kb_data <<= 8;
            }
            else
            {
                QueuePut(kb_data);
                kb_data = 0;
            }
        }
        break;
    }
}

__attribute__((section(".RamFunc")))
void PS2_PROCESS()
{
    uint32_t gpioBits = GPIOB->IDR;
    uint16_t clkBit = (gpioBits & CLK_PIN);
    uint8_t dataBit = (gpioBits & DATA_PIN) ? 1 : 0;

    if (clkBit == 0)
    {
        // CLK = 0

        lastData = dataBit;
        lastClk = 0;
    }
    else
    {
        // CLK = 1

        if (lastClk == 0)
        {
            Update(lastData);
            lastClk = 1;
        }
        else
        {
            if (ps2_status != IDLE)
            {
                lastClk++;
                if (lastClk > 10)
                {
                    // Timeout
                    ps2_status = IDLE;
                    kb_data = 0;

                    // TODO ask to resend
                }
            }
            else
            {
                lastClk = 1;
            }
        }
    }
}
