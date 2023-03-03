#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <stdarg.h>


//-----------------------------------------------------------------------------------------------
constexpr int STRINGF_STACK_LOCAL_TEMP_LENGTH = 2048;


//-----------------------------------------------------------------------------------------------
const std::string Stringf( char const* format, ... )
{
	char textLiteral[ STRINGF_STACK_LOCAL_TEMP_LENGTH ];
	va_list variableArgumentList;
	va_start( variableArgumentList, format );
	vsnprintf_s( textLiteral, STRINGF_STACK_LOCAL_TEMP_LENGTH, _TRUNCATE, format, variableArgumentList );	
	va_end( variableArgumentList );
	textLiteral[ STRINGF_STACK_LOCAL_TEMP_LENGTH - 1 ] = '\0'; // In case vsnprintf overran (doesn't auto-terminate)

	return std::string( textLiteral );
}


//-----------------------------------------------------------------------------------------------
const std::string Stringf( int maxLength, char const* format, ... )
{
	char textLiteralSmall[ STRINGF_STACK_LOCAL_TEMP_LENGTH ];
	char* textLiteral = textLiteralSmall;
	if( maxLength > STRINGF_STACK_LOCAL_TEMP_LENGTH )
		textLiteral = new char[ maxLength ];

	va_list variableArgumentList;
	va_start( variableArgumentList, format );
	vsnprintf_s( textLiteral, maxLength, _TRUNCATE, format, variableArgumentList );	
	va_end( variableArgumentList );
	textLiteral[ maxLength - 1 ] = '\0'; // In case vsnprintf overran (doesn't auto-terminate)

	std::string returnValue( textLiteral );
	if( maxLength > STRINGF_STACK_LOCAL_TEMP_LENGTH )
		delete[] textLiteral;

	return returnValue;
}

Strings SplitStringOnDelimiter(const std::string& originalString, char delimiterToSplitOn, bool ignoreDelimiterInsideQuotes)
{
	Strings splitStrings;

	bool insideQuotes = false;
	int lastDelimiterIndex = 0;
	for (int i = 0; i < originalString.size(); i++)
	{
		if (originalString[i] == '"')
			insideQuotes = !insideQuotes;

		if (originalString[i] == delimiterToSplitOn)
		{
			if(ignoreDelimiterInsideQuotes && insideQuotes)
				continue;
			std::string subString = originalString.substr(lastDelimiterIndex, i - lastDelimiterIndex);
			splitStrings.push_back(subString);
			lastDelimiterIndex = i + 1;
		}

	}
	std::string subString = originalString.substr(lastDelimiterIndex, originalString.size() - lastDelimiterIndex);
	splitStrings.push_back(subString);

	return splitStrings;
}

void RemoveLeadingAndTrailingWhiteSpaces(std::string& stringToClean)
{
	std::string copy = stringToClean;
	int firstNonSpaceCharacterPos = static_cast<int>(copy.find_first_not_of(' '));
	int lastNonSpaceCharacterPos = static_cast<int>(copy.find_last_not_of(' '));
	stringToClean = copy.substr(firstNonSpaceCharacterPos, lastNonSpaceCharacterPos - firstNonSpaceCharacterPos + 1);
}

void RemoveAllQuotation(std::string& stringToClean)
{
	std::string cleanString;
	for (int i = 0; i < stringToClean.size(); i++)
	{
		if(stringToClean[i] == '"')
			continue;

		cleanString.push_back(stringToClean[i]);
	}
	stringToClean = cleanString;
}

std::string GetLongestStringAmongstStrings(Strings stringsToCheck)
{
	std::string longestString = "";
	for (int i = 0; i < stringsToCheck.size(); i++)
	{
		if (stringsToCheck[i].length() > longestString.length())
			longestString = stringsToCheck[i];
	}

	return longestString;
}


