/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#include "rt11dsk.h"

/*
The question of how to convert rad50 to ascii seems to
appear every couple years.  I upload these C routines
which I use to decode RT11 directory information on a PC.
They are generic and serve as an example.  The text below is
from a message response a couple years ago.
Will Kranz 70402,423

For info on rad50 see appendix A of Macro 11 manual in
RT11 documentation set.

   The system library on a PDP11 system should contain two
   conversion routines:

   r50asc(icnt,in,out)   from rad50 to ascii

   irad50(icnt,in,out)   from ascii to rad50

   where icnt = # of chars to convert
         in and out are buffers.  The typical destination
         for rad50 stuff in fortran is a real*4

   For each group of 3 chars:
   rad50 = (1st char) *40 *40 + (2nd char) * 40 + 3rd char

   Note only a limited 40 character set is supported for this
   to work, {' ','A'-'Z','$','.','0'-'9'}.

   Per your original request, the example from the manual
   is as follows   (back to Octal for Dec consistency)
   string = "X2b"
       X = 113000
       2 = 002400
       B = 000002
     "X2B" = 115402   in octal rad50, What a space savings!
              I'm happy memory is cheap and I don't need to do this.

binary    1 001 101 100 000 010
          1001 1011 0000 0010     => 9B02

test_rad()
{
    char buf[10];
    unsigned v = 0x9b02;
    r50asc(3,&v,buf);
    puts(buf);  should be "X2B"
}

*/


/* Convert
Passed the number of characters to unpack, the array of words to convert back to char,
and the output string str[] */

void r50asc(int cnt, uint16_t* r50, char str[])
{
    /* sorry I think in decimal, 39 = Octal 47, decimal, 40 = Octal 50 */
    int i;
    for (i = 0; i < cnt; i++)
    {
        unsigned int v, ch, ord, word = 0;

        /* get 3 chars from each word */
        word = i / 3;
        v = r50[word];
        ord = 2 - (i % 3);

        while (ord-- > 0)
        {
            v /= 40;
        }

        v %= 40; /* mask all but bits of interest */
        if (v == 0)                      ch = ' ';      /* space */
        else if (v >= 1 && v <= 26)    ch = v - 1 + 'A';    /* printable */
        else if (v == 27)              ch = '$';
        else if (v == 28)              ch = '.';
        else if (v == 29)              ch = 255;        /* unused ! */
        else if (v >= 30 && v <= 39)   ch = v - 30 + '0';   /* digit */
        /* end of valid RAD50 range, display table values */

        str[i] = ch;
    }
    str[i] = 0; /* always nul terminate */
}

/* number of chars to pack */ /* input string */ /* array of words to fill from char */
void irad50( int cnt, char str[], uint16_t r50[] )
{
    unsigned int v = 0;
    int i;
    /* sorry I think in decimal, 39 = Octal 47,
                        decimal, 40 = Octal 50
    */
    for (i = 0; i < cnt; i++)
    {
        if (str[i] == ' ')                       v = 0; /* space */
        else if (str[i] >= 'A' &&
                str[i] <= 'Z')  v = str[i] - 'A' + 1; /* printable */
        else if (str[i] == '$') v = 27;
        else if (str[i] == '.') v = 28;
        else if (str[i] >= '0' &&
                str[i] <= '9')  v = str[i] - '0' + 30; /* digit */
        /* end of valid RAD50 range, display table values */

        if ((i % 3) == 0)
            r50[i / 3] = v * 1600; /* will clear all bits */
        else if ((i % 3) == 1)
            r50[i / 3] += v * 40;
        else
            r50[i / 3] += v ;

        /* put 3 chars into each word */
    }
}


/* */

void rtDateStr(uint16_t date, char* str)
{
    const char* months[] =
    { "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    int year  = (date & 0x1F) + 72;
    int day   = (date >> 5)  & 0x1F;
    int month = (date >> 10) & 0x1F;

    if (month < 1 || month > 12)
        strcpy_s(str, 10, "  -BAD-  ");
    else
        sprintf_s(str, 10, "%02d-%3S-%02d", day, months[month], year );
}
