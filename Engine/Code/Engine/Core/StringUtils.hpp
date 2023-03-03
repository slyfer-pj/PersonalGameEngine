#pragma once
//-----------------------------------------------------------------------------------------------
#include <string>
#include <vector>


//-----------------------------------------------------------------------------------------------
const std::string Stringf( char const* format, ... );
const std::string Stringf( int maxLength, char const* format, ... );

typedef std::vector<std::string> Strings;
Strings SplitStringOnDelimiter(const std::string& originalString, char delimiterToSplitOn, bool ignoreDelimiterInsideQuotes = false);
void RemoveLeadingAndTrailingWhiteSpaces(std::string& stringToClean);
void RemoveAllQuotation(std::string& stringToClean);
std::string GetLongestStringAmongstStrings(Strings stringsToCheck);



