#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "encode.h"

#define __STD_UTF_16__

//Pointer arrays must always include the array size, because pointers do not know about the size of the supposed array size.
void utf8_to_utf16(unsigned char*  utf8_str, int utf8_str_size, uint16_t * utf16_str_output, int utf16_str_output_size) {
    //First, grab the first byte of the UTF-8 string
    unsigned char* utf8_currentCodeUnit = utf8_str;
    uint16_t* utf16_currentCodeUnit = utf16_str_output;
    int utf8_str_iterator = 0;
    int utf16_str_iterator = 0;

    //In a while loop, we check if the UTF-16 iterator is less than the max output size. If true, then we check if UTF-8 iterator
    //is less than UTF-8 max string size. This conditional checking based on order of precedence is intentionally done so it
    //prevents the while loop from continuing onwards if the iterators are outside of the intended sizes.
    while (*utf8_currentCodeUnit && (utf16_str_iterator < utf16_str_output_size || utf8_str_iterator < utf8_str_size)) {
        //Figure out the current code unit to determine the range. It is split into 6 main groups, each of which handles the data
        //differently from one another.
        if (*utf8_currentCodeUnit < 0x80) {
            //0..127, the ASCII range.

            //We directly plug in the values to the UTF-16 code unit.
            *utf16_currentCodeUnit = (uint16_t) (*utf8_currentCodeUnit);
            utf16_currentCodeUnit++;
            utf16_str_iterator++;

            //Increment the current code unit pointer to the next code unit
            utf8_currentCodeUnit++;

            //Increment the iterator to keep track of where we are in the UTF-8 string
            utf8_str_iterator++;
        }
        else if (*utf8_currentCodeUnit < 0xC0) {
            //0x80..0xBF, we ignore. These are reserved for UTF-8 encoding.
            utf8_currentCodeUnit++;
            utf8_str_iterator++;
        }
        else if (*utf8_currentCodeUnit < 0xE0) {
            //128..2047, the extended ASCII range, and into the Basic Multilingual Plane.

            //Work on the first code unit.
            uint16_t highShort = (uint16_t) ((*utf8_currentCodeUnit) & 0x1F);

            //Increment the current code unit pointer to the next code unit
            utf8_currentCodeUnit++;

            //Work on the second code unit.
            uint16_t lowShort = (uint16_t) ((*utf8_currentCodeUnit) & 0x3F);

            //Increment the current code unit pointer to the next code unit
            utf8_currentCodeUnit++;

            //Create the UTF-16 code unit, then increment the iterator.
            int unicode = (highShort << 6) | lowShort;

            //Check to make sure the "unicode" is in the range [0..D7FF] and [E000..FFFF].
            if ((0 <= unicode && unicode <= 0xD7FF) || (0xE000 <= unicode && unicode <= 0xFFFF)) {
                //Directly set the value to the UTF-16 code unit.
                *utf16_currentCodeUnit = (uint16_t) unicode;
                utf16_currentCodeUnit++;
                utf16_str_iterator++;
            }

            //Increment the iterator to keep track of where we are in the UTF-8 string
            utf8_str_iterator += 2;
        }
        else if (*utf8_currentCodeUnit < 0xF0) {
            //2048..65535, the remaining Basic Multilingual Plane.

            //Work on the UTF-8 code units one by one.
            //If drawn out, it would be 1110aaaa 10bbbbcc 10ccdddd
            //Where a is 4th byte, b is 3rd byte, c is 2nd byte, and d is 1st byte.
            uint16_t fourthChar = (uint16_t) ((*utf8_currentCodeUnit) & 0xF);
            utf8_currentCodeUnit++;
            uint16_t thirdChar = (uint16_t) ((*utf8_currentCodeUnit) & 0x3C) >> 2;
            uint16_t secondCharHigh = (uint16_t) ((*utf8_currentCodeUnit) & 0x3);
            utf8_currentCodeUnit++;
            uint16_t secondCharLow = (uint16_t) ((*utf8_currentCodeUnit) & 0x30) >> 4;
            uint16_t firstChar = (uint16_t) ((*utf8_currentCodeUnit) & 0xF);
            utf8_currentCodeUnit++;

            //Create the resulting UTF-16 code unit, then increment the iterator.
            int unicode = (fourthChar << 12) | (thirdChar << 8) | (secondCharHigh << 6) | (secondCharLow << 4) | firstChar;

            //Check to make sure the "unicode" is in the range [0..D7FF] and [E000..FFFF].
            //According to math, UTF-8 encoded "unicode" should always fall within these two ranges.
            if ((0 <= unicode && unicode <= 0xD7FF) || (0xE000 <= unicode && unicode <= 0xFFFF)) {
                //Directly set the value to the UTF-16 code unit.
                *utf16_currentCodeUnit = (uint16_t) unicode;
                utf16_currentCodeUnit++;
                utf16_str_iterator++;
            }

            //Increment the iterator to keep track of where we are in the UTF-8 string
            utf8_str_iterator += 3;
        }
        else if (*utf8_currentCodeUnit < 0xF8) {
            //65536..10FFFF, the Unicode UTF range

            //Work on the UTF-8 code units one by one.
            //If drawn out, it would be 11110abb 10bbcccc 10ddddee 10eeffff
            //Where a is 6th byte, b is 5th byte, c is 4th byte, and so on.
            uint16_t sixthChar = (uint16_t) ((*utf8_currentCodeUnit) & 0x4) >> 2;
            uint16_t fifthCharHigh = (uint16_t) ((*utf8_currentCodeUnit) & 0x3);
            utf8_currentCodeUnit++;
            uint16_t fifthCharLow = (uint16_t) ((*utf8_currentCodeUnit) & 0x30) >> 4;
            uint16_t fourthChar = (uint16_t) ((*utf8_currentCodeUnit) & 0xF);
            utf8_currentCodeUnit++;
            uint16_t thirdChar = (uint16_t) ((*utf8_currentCodeUnit) & 0x3C) >> 2;
            uint16_t secondCharHigh = (uint16_t) ((*utf8_currentCodeUnit) & 0x3);
            utf8_currentCodeUnit++;
            uint16_t secondCharLow = (uint16_t) ((*utf8_currentCodeUnit) & 0x30) >> 4;
            uint16_t firstChar = (uint16_t) ((*utf8_currentCodeUnit) & 0xF);
            utf8_currentCodeUnit++;

            int unicode = (sixthChar << 4) | (fifthCharHigh << 2) | fifthCharLow | (fourthChar << 12) | (thirdChar << 8) | (secondCharHigh << 6) | (secondCharLow << 4) | firstChar;
            uint16_t highSurrogate = (unicode - 0x10000) / 0x400 + 0xD800;
            uint16_t lowSurrogate = (unicode - 0x10000) % 0x400 + 0xDC00;

            //Set the UTF-16 code units
            *utf16_currentCodeUnit = lowSurrogate;
            utf16_currentCodeUnit++;
            utf16_str_iterator++;

            //Check to see if we're still below the output string size before continuing, otherwise, we cut off here.
            if (utf16_str_iterator < utf16_str_output_size) {
                *utf16_currentCodeUnit = highSurrogate;
                utf16_currentCodeUnit++;
                utf16_str_iterator++;
            }

            //Increment the iterator to keep track of where we are in the UTF-8 string
            utf8_str_iterator += 4;
        }
        else {
            //Invalid UTF-8 code unit, we ignore.
            utf8_currentCodeUnit++;
            utf8_str_iterator++;
        }
    }

    //We clean up the output string if the UTF-16 iterator is still less than the output string size.
    while (utf16_str_iterator < utf16_str_output_size) {
        *utf16_currentCodeUnit = '\0';
        utf16_currentCodeUnit++;
        utf16_str_iterator++;
    }
}