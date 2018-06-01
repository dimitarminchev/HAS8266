
#include <Arduino.h>
#include <string.h>
#include "IniFile.h"

// Constructor
IniFile::IniFile(fs::FS &fs, const char* filename)
{
  if (_file) _file.close();
  _file = fs.open(filename, "r");
}

// Destructor
IniFile::~IniFile()
{
  if (_file) _file.close();
}

// Validate
bool IniFile::validate(char* buffer, size_t len)
{
  uint32_t pos = 0;
  error_t err;
  while ((err = readLine(buffer, len, pos)) == errorNoError) ;;
  if (err == errorEndOfFile) {
    _error = errorNoError;
    return true;
  }
  else {
    _error = err;
    return false;
  }
}

// Get Value
bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, IniFileState &state)
{
  bool done = false;
  if (!_file) {
    _error = errorFileNotOpen;
    return true;
  }

  switch (state.getValueState) {
    case IniFileState::funcUnset:
      state.getValueState = (section == NULL ? IniFileState::funcFindKey : IniFileState::funcFindSection);
      state.readLinePosition = 0;
      break;

    case IniFileState::funcFindSection:
      if (findSection(section, buffer, len, state)) {
        if (_error != errorNoError) return true;
        state.getValueState = IniFileState::funcFindKey;
      }
      break;

    case IniFileState::funcFindKey:
      char *cp;
      if (findKey(section, key, buffer, len, &cp, state)) {
        if (_error != errorNoError)
          return true;
        // Found key line in correct section
        cp = skipWhiteSpace(cp);
        removeTrailingWhiteSpace(cp);

        // Copy from cp to buffer, but the strings overlap so strcpy is out
        while (*cp != '\0') *buffer++ = *cp++;
        *buffer = '\0';
        return true;
      }
      break;

    default:
      // How did this happen?
      _error = errorUnknownError;
      done = true;
      break;
  }

  return done;
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len)
{
  IniFileState state;
  while (!getValue(section, key, buffer, len, state)) ;;
  return _error == errorNoError;
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, char *value, size_t vlen)
{
  if (getValue(section, key, buffer, len) < 0) return false; // error
  if (strlen(buffer) >= vlen) return false;
  strcpy(value, buffer);
  return true;
}

// For true accept: true, yes, 1
// For false accept: false, no, 0
bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, bool& val)
{
  if (getValue(section, key, buffer, len) < 0) return false; // error
  if (strcasecmp(buffer, "true") == 0 ||
      strcasecmp(buffer, "yes") == 0 ||
      strcasecmp(buffer, "1") == 0) {
    val = true;
    return true;
  }
  if (strcasecmp(buffer, "false") == 0 ||
      strcasecmp(buffer, "no") == 0 ||
      strcasecmp(buffer, "0") == 0) {
    val = false;
    return true;
  }
  return false; // does not match any known strings
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, int& val)
{
  if (getValue(section, key, buffer, len) < 0) return false; // error
  val = atoi(buffer);
  return true;
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, uint16_t& val)
{
  long longval;
  bool r = getValue(section, key, buffer, len, longval);
  if (r) val = uint16_t(longval);
  return r;
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, long& val)
{
  if (getValue(section, key, buffer, len) < 0) return false; // error
  val = atol(buffer);
  return true;
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, unsigned long& val)
{
  if (getValue(section, key, buffer, len) < 0) return false; // error
  char *endptr;
  unsigned long tmp = strtoul(buffer, &endptr, 10);
  if (endptr == buffer) return false; // no conversion
  if (*endptr == '\0') {
    val = tmp;
    return true; // valid conversion
  }
  // buffer has trailing non-numeric characters, and since the buffer
  // already had whitespace removed discard the entire results
  return false;
}

bool IniFile::getValue(const char* section, const char* key, char* buffer, size_t len, float & val)
{
  if (getValue(section, key, buffer, len) < 0) return false; // error
  char *endptr;
  float tmp = strtod(buffer, &endptr);
  if (endptr == buffer) return false; // no conversion
  if (*endptr == '\0') {
    val = tmp;
    return true; // valid conversion
  }
  // buffer has trailing non-numeric characters, and since the buffer
  // already had whitespace removed discard the entire results
  return false;
}

// Utility function to read a line from a file, make available to all static
// int8_t readLine(File &file, char *buffer, size_t len, uint32_t &pos);
IniFile::error_t IniFile::readLine(char *buffer, size_t len, uint32_t &pos)
{
  if (!_file) return errorFileNotOpen;
  if (len < 3) return errorBufferTooSmall;
  if (!_file.seek(pos, SeekSet)) return errorSeekError;

  // minchev
  size_t bytesRead = 0;
  for (int k = 0; k < len; k++)
  {
    char c = _file.read();
    buffer[bytesRead++] = c;
  }
  //size_t bytesRead = _file.read(buffer, len);

  if (!bytesRead) {
    buffer[0] = '\0';
    //return 1; // done
    return errorEndOfFile;
  }

  for (size_t i = 0; i < bytesRead && i < len - 1; ++i) {
    // Test for '\n' with optional '\r' too
    // if (endOfLineTest(buffer, len, i, '\n', '\r')

    if (buffer[i] == '\n' || buffer[i] == '\r') {
      char match = buffer[i];
      char otherNewline = (match == '\n' ? '\r' : '\n');
      // end of line, discard any trailing character of the other sort
      // of newline
      buffer[i] = '\0';

      if (buffer[i + 1] == otherNewline) ++i;
      pos += (i + 1); // skip past newline(s)
      //return (i+1 == bytesRead && !file.available());
      return errorNoError;
    }
  }
  if (!_file.available()) {
    // end of file without a newline
    buffer[bytesRead] = '\0';
    // return 1; //done
    return errorEndOfFile;
  }

  buffer[len - 1] = '\0'; // terminate the string
  return errorBufferTooSmall;
}

bool IniFile::isCommentChar(char c)
{
  return (c == ';' || c == '#');
}

char* IniFile::skipWhiteSpace(char* str)
{
  char *cp = str;
  while (isspace(*cp)) ++cp;
  return cp;
}

void IniFile::removeTrailingWhiteSpace(char* str)
{
  char *cp = str + strlen(str) - 1;
  while (cp >= str && isspace(*cp)) *cp-- = '\0';
}

// Find Section
bool IniFile::findSection(const char* section, char* buffer, size_t len, IniFileState &state)
{
  if (section == NULL) {
    _error = errorSectionNotFound;
    return true;
  }

  error_t err = IniFile::readLine( buffer, len, state.readLinePosition);

  if (err != errorNoError && err != errorEndOfFile) {
    // Signal to caller to stop looking and any error value
    _error = err;
    return true;
  }

  char *cp = skipWhiteSpace(buffer);
  if (isCommentChar(*cp)) {
    // return (err == errorEndOfFile ? errorSectionNotFound : errorNoError);
    if (err == errorSectionNotFound) {
      _error = err;
      return true;
    }
    else return false; // Continue searching
  }

  if (*cp == '[') {
    // Start of section
    ++cp;
    cp = skipWhiteSpace(cp);
    char *ep = strchr(cp, ']');
    if (ep != NULL) {
      *ep = '\0'; // make ] be end of string
      removeTrailingWhiteSpace(cp);
      if (strcasecmp(cp, section) == 0)
      {
        _error = errorNoError;
        return true;
      }
    }
  }

  // Not a valid section line
  // return (done ? errorSectionNotFound : 0);
  if (err == errorEndOfFile) {
    _error = errorSectionNotFound;
    return true;
  }
  return false;
}

// From the current file location look for the matching key. If section is non-NULL don't look in the next section
bool IniFile::findKey(const char* section, const char* key, char* buffer, size_t len, char** keyptr, IniFileState &state)
{
  if (key == NULL || *key == '\0') {
    _error = errorKeyNotFound;
    return true;
  }

  error_t err = readLine(buffer, len, state.readLinePosition);
  if (err != errorNoError && err != errorEndOfFile) {
    _error = err;
    return true;
  }

  char *cp = skipWhiteSpace(buffer);
  if (isCommentChar(*cp)) {
    if (err == errorEndOfFile) {
      _error = errorKeyNotFound;
      return true;
    }
    else return false; // Continue searching
  }

  if (section && *cp == '[') {
    // Start of a new section
    _error = errorKeyNotFound;
    return true;
  }

  // Find '='
  char *ep = strchr(cp, '=');
  if (ep != NULL) {
    *ep = '\0'; // make = be the end of string
    removeTrailingWhiteSpace(cp);
    if (strcasecmp(cp, key) == 0)
    {
      *keyptr = ep + 1;
      _error = errorNoError;
      return true;
    }
  }

  // Not the valid key line
  if (err == errorEndOfFile) {
    _error = errorKeyNotFound;
    return true;
  }
  return false;
}

// Return Error Message
String IniFile::printError(uint8_t e)
{
  switch (e)
  {
    case errorNoError: return "no error";
    case errorFileNotFound: return "file not found";
    case errorFileNotOpen: return "file not open";
    case errorBufferTooSmall: return "buffer too small";
    case errorSeekError: return "seek error";
    case errorSectionNotFound: return "section not found";
    case errorKeyNotFound: return "key not found";
    case errorEndOfFile: return "end of file";
    case errorUnknownError: return "unknown error";
    default: return "unknown error value";
  }
}


