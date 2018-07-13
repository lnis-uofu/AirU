/*
 * app_utils.c
 *
 *  Created on: Jun 22, 2017
 *      Author: Tom
 */

#include "app_utils.h"

const char pcDigits[] = "0123456789"; /* variable used by itoa function */


/******************************************
 * Sort a and put new indices in b.
 *
 * Does not sort a
 */
void argsort(int* a, int* b, int n)
{
    int i, j, t,v;

    for (i=0 ; i<n ; i++)
    {
        b[i] = i;
    }

    for (j=0 ; j<(n-1) ; j++)
    {
        for (i=0 ; i<(n-1) ; i++)
        {
            if (a[i+1] > a[i])
            {
                t = a[i];
                a[i] = a[i+1];
                a[i+1] = t;

                v = b[i];
                b[i] = b[i+1];
                b[i+1] = v;
            }
        }
    }
}

/*
 * Slow sort! Sorts a in place
 */
void ssort(int* a, int n)
{
    int i, j, t;

    for (j=0 ; j<(n-1) ; j++)
    {
        for (i=0 ; i<(n-1) ; i++)
        {
            if (a[i+1] > a[i])
            {
                t = a[i];
                a[i] = a[i + 1];
                a[i + 1] = t;
            }
        }
    }
}

//*****************************************************************************
//
//! itoa
//!
//!    @brief  Convert integer to ASCII in decimal base
//!
//!     @param  cNum is input integer number to convert
//!     @param  cString is output string
//!
//!     @return number of ASCII parameters
//!
//!     @note can't do negative numbers
//
//*****************************************************************************
unsigned short itoa(unsigned short cNum, char *cString)
{
    char* ptr;
    short uTemp = cNum;
    unsigned short length;

    if (cNum < 0)
    {
        *cString++ = '-';
    }
    // single digit: append zero
    if (cNum < 10)
    {
        length = 2;
        *cString++ = '0';
        *cString = pcDigits[cNum % 10];
        return length;
    }


    // Find out the length of the number, in decimal base
    length = 0;
    while (uTemp > 0)
    {
        uTemp /= 10;
        length++;
    }

    // Do the actual formatting, right to left
    ptr = cString + length;

    uTemp = cNum;

    while (uTemp > 0)
    {
        --ptr;
        *ptr = pcDigits[uTemp % 10];
        uTemp /= 10;
    }

    return length;
}

//*****************************************************************************
//
//! stripChar
//!
//!    \brief   Removes all occurances of a character from a string
//!
//!     \param  cStringDst is the destination string to write to
//!     \param  cStringSrc is the source string to pull from
//!
//!     \return N/A
//!
//*****************************************************************************
void stripChar(char *cStringDst, char *cStringSrc, char c){
    char *pr = cStringSrc;
    char *pw = cStringDst;

    while(*pr){
        *pw = *pr++;
        pw += (*pw != c);
    }
    *pw = '\0';
}

//*****************************************************************************
//
//! mod
//!
//!    \brief   True modulus operator (for use with negative numbers)
//!
//!     \param  a in a % b
//!     \param  b in a % b
//!
//!     \return a % b
//!
//*****************************************************************************
int mod (int a, int b)
{
   int ret = a % b;
   if(ret < 0)
     ret+=b;
   return ret;
}

// read a Hex value and return the decimal equivalent
int parseHex(char c) {
    if (c < '0')
      return 0;
    if (c <= '9')
      return c - '0';
    if (c < 'A')
       return 0;
    if (c <= 'F')
       return (c - 'A')+10;
    // if (c > 'F')
    return 0;
}
