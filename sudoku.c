/*
Autor:	Ing. Martin Schwarz, studio@studio-schwarz.cz, T: +420736541242
Date:	23.04.2024
Language: C

Working version, can be compiled and run as an application, communication over the USART2 serial line is only simulated - see main() 
*/


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

#define USART_ISR_RXNE (1 << 5)
#define USART_ISR_TC (1 << 6)

// size of sudoku (numbers only !)
#define SUDOKU_ROW_LENGTH 9 
// stream length = numbers + delimiters
#define SUDOKU_SRC_LEN ((SUDOKU_ROW_LENGTH + (SUDOKU_ROW_LENGTH / 3 - 1)) * ((SUDOKU_ROW_LENGTH + (SUDOKU_ROW_LENGTH / 3 - 1)) + 2))

// #define IS_NUMBER(x) ((x) >= '0' && (x) <= '9')	// replaced by isdigit()

// only for simulating communication on UART - IRQhandler use this struct
struct Usart {
   uint16_t ISR;
   uint8_t RDR;
} myUSART2;

struct Usart* USART2 = &myUSART2;

// test stream
const char serialData[] = {"\
530|070|000\r\n600|195|000\r\n098|000|060\r\n-----------\r\n\
800|060|003\r\n400|803|001\r\n700|020|006\r\n-----------\r\n\
060|000|280\r\n000|419|005\r\n000|080|070\r\n"};

char receivedData[SUDOKU_SRC_LEN] = "\0";
bool receivedDataAck = true;	// Are the received data OK ? We believe so.

//bitarrays
uint16_t submatrixDigits[SUDOKU_ROW_LENGTH] = {0};
uint16_t columnDigits[SUDOKU_ROW_LENGTH] = {0};
uint16_t rowDigits[SUDOKU_ROW_LENGTH] = {0};

// the numerical form of sudoku
uint8_t sudokuBoard[SUDOKU_ROW_LENGTH][SUDOKU_ROW_LENGTH];

// some statistics
int level = 0;	// recursion level
int calls = 0;	// nr of iterations
int maxRecursions = 0;	// max. recursion level

// declaration of procedures
void USART2_IRQHandler(void);
bool solve(uint8_t, uint8_t);
bool SolveSudoku(const char *);
void printGrid(const char*);
void printBoard(void);
void delay(int);

// delay in ms
void delay(int ms)
{
    clock_t start_time = clock();
    while (clock() < start_time + ms);
}


void printGrid(const char* Grid)
{
	puts(Grid);
}

void printBoard(void)  
{  
	for (uint8_t row = 0; row < SUDOKU_ROW_LENGTH; row++)  
	{  
		if (row > 0 && !(row % 3)) printf("%.*s\n", SUDOKU_ROW_LENGTH + (SUDOKU_ROW_LENGTH / 3 - 1), "------------------------------------");
		
		for (uint8_t col = 0; col < SUDOKU_ROW_LENGTH; col++) { 
			if (col > 0 && !(col % 3)) printf("|");
			printf("%d", sudokuBoard[row][col]);
			if ((col + 1) == SUDOKU_ROW_LENGTH) printf("\n");
		}
	}  
}


// serial communication by the interupt 
void USART2_IRQHandler(void) { 
	static uint8_t cnt = 0; // received bytes counter
	static uint8_t nrs = 0; // received numbers counter

	if((USART2->ISR & USART_ISR_RXNE) == USART_ISR_RXNE ) { 
		uint8_t ch = USART2->RDR;
		// while ((USART2->ISR & USART_ISR_TC) != USART_ISR_TC);    // is it necessary test the TC bit ??
		receivedData[cnt++] = ch;  
		if (isdigit(ch)) { ++nrs; } 
		if (cnt == SUDOKU_SRC_LEN || nrs == (SUDOKU_ROW_LENGTH * SUDOKU_ROW_LENGTH)) {   // did we receive all data ?
			if (nrs != (SUDOKU_ROW_LENGTH * SUDOKU_ROW_LENGTH)) {  //  if we didn't receive all numbers ...
				printf("\nChyba: Přišlo %d číslic z předpokládaných %d. Sudoku nelze vyřešit.\n\n", nrs, (SUDOKU_ROW_LENGTH * SUDOKU_ROW_LENGTH));
				receivedDataAck = false;	// set received data acknowledgment to false
			}
			// we received data, set static variables to default and try to solve the sudoku
			cnt = 0;
			nrs = 0;
			if (SolveSudoku(receivedData)) {
				puts("\nReseni");						// we found a solution, so print it out
				printf("Pocet volani: %d, max. uroven rekurze: %d.\n\n", calls, maxRecursions);	// statictics
				printGrid(receivedData);
				puts(" ");
/*				switch (USART_PrintSudokuGrid(receivedData, SUDOKU_ROW_LENGTH)) {	// volání vaší fce pro tisk výsledku a ošetření návratové hodnoty
					case 1:
						puts("\nChyba: Sudoku není zarovnané v mřížce (oddělení 3x3 bloků)\n\n");
						break;
					case 2:
						puts("\nChyba: Příliš krátké řešení sudoku (narazili jsme na null terminátor)\n\n");
						break;
					default:
						break;
				}
*/
			}
			else {
				printf("\nŘesení nenalezeno.\n\n");
			}
		}
		// cnt++;
	} 
} 

/*
A function to send a solved Sudoku away over a serial line
The function calculates the text form of the solved sudoku
The return value is zero if the send was successful
The return value is > 0 if an error occurs:
1 = Sudoku is not aligned in the grid (separation of 3x3 blocks)
2 = Sudoku solution too short (we ran into a null terminator)

uint8_t USART_PrintSudokuGrid(const char *Grid, uint8_t RowLength);
*/


bool solve(uint8_t r, uint8_t c)		//r row, c column
{
	++level;	// recursion level counter
	++calls;	// iterations counter
	printf("|"); 	// recursion level bar
	fflush(stdout);
	delay(1000);	// delay to show recursion level bar
	maxRecursions = (level > maxRecursions)? level:maxRecursions;

	if (r == SUDOKU_ROW_LENGTH)	{		// all rows have been processed so return true
		--level;
		printf("\b \b"); fflush(stdout);
		return true;
	}
	
	if (c == SUDOKU_ROW_LENGTH)	{		// all columns have been processed, so go to the next row
		--level;
		printf("\b \b"); fflush(stdout);
		return solve(r + 1, 0);
	}
	if (sudokuBoard[r][c] == 0) {		// unsolved cell ?? 
		for (uint8_t i = 1; i <= SUDOKU_ROW_LENGTH; i++)
		{
			uint16_t digit = 1 << (i - 1);
			
			if (!((submatrixDigits[(r / 3) * 3 + c / 3] & digit)	// can we use this number ??
				  || (rowDigits[r] & digit) 
				  || (columnDigits[c] & digit)))
			{
				submatrixDigits[(r / 3) * 3 + c / 3] |= digit;		// yes, we will register the number
				rowDigits[r] |= digit;
				columnDigits[c] |= digit;
				sudokuBoard[r][c] = i;	
				
				if (solve(r, c + 1)) {   // and continue with the next column
					--level;
					printf("\b \b"); fflush(stdout);
					return true;
				}
				else
				{
					submatrixDigits[(r / 3) * 3 + c / 3] &= ~digit;		// the number was wrong, we need to unregister it 
					rowDigits[r] &= ~digit;
					columnDigits[c] &= ~digit;
					sudokuBoard[r][c] = 0;		// and set the cell back to zero
				}
			}
		}
		--level;
		printf("\b \b"); fflush(stdout);
		return false;		// the current combination is not correct
	}
	--level;
	printf("\b \b"); fflush(stdout);
	return solve(r, c + 1);  // continue with the next column
}

// function for solving sudoku
bool SolveSudoku(const char *Grid)
{
	puts("Zadani\n");	
	printGrid(receivedData);

	if (!receivedDataAck) { 	// from fnc USART2_IRQHandler() - data are not correct
		receivedDataAck = true;	// set to default value
		return false;
	}

	uint8_t cnt = 0;				// convert stream to numeric array
	uint8_t nrs = 0;
	char ch;
	while((ch = *(Grid + cnt)) != '\0') {
		if (isdigit(ch)) {
			sudokuBoard[nrs / SUDOKU_ROW_LENGTH][nrs % SUDOKU_ROW_LENGTH] = ch - '0';
			++nrs;
		}
		++cnt;
	}

	// map received data to bitarray
	for (uint8_t i = 0; i < SUDOKU_ROW_LENGTH; i++) {
		for (uint8_t j = 0; j < SUDOKU_ROW_LENGTH; j++) {
			if (sudokuBoard[i][j] > 0)
			{
				uint16_t value = 1 << (sudokuBoard[i][j] - 1);
				submatrixDigits[(i / 3) * 3 + j / 3] |= value;
				rowDigits[i] |= value;
				columnDigits[j] |= value;
			}
		}
	}

	if (solve(0, 0)) {		// start solving sudoku from top left corner
		// we found solution
		// convert numeric sudoku back to stream
		char * const ptr = Grid;
		for (uint8_t i = 0; i < SUDOKU_ROW_LENGTH; i++) {
			for (uint8_t j = 0; j < SUDOKU_ROW_LENGTH; j++) {
				*(ptr + (i + i / 3) * (SUDOKU_ROW_LENGTH + 4) + (j + j / 3)) = (char)sudokuBoard[i][j] + '0';
			}
		}
		return true;
	}
	else {		// solution doesn't exists
		return false;
	}
}

int main(void)  
{  
	// simulation of serial communication
	USART2->ISR = 0 | USART_ISR_RXNE | USART_ISR_TC;
	for (uint8_t i = 0; i < SUDOKU_SRC_LEN; i++) {
		USART2->RDR = serialData[i];
		USART2_IRQHandler();
	}

	return 0;  
}


