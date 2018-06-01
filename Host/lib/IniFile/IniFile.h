#ifndef _INI_FILE_H
#define _INI_FILE_H

// Headers
#include <Arduino.h>
#include "FS.h"

// Class Ini File State
class IniFileState
{
  public:
    IniFileState()
    {
      readLinePosition = 0;
      getValueState = funcUnset;
    };
  private:
    enum
    {
      funcUnset = 0,
      funcFindSection,
      funcFindKey,
    };
    uint32_t readLinePosition;
    uint8_t getValueState;
    friend class IniFile;
};

// Class Ini File
class IniFile
{
    // Public Members
  public:
    IniFile(fs::FS &fs, const char* filename);
    ~IniFile();

    // Errors
    enum error_t
    {
      errorNoError = 0,
      errorFileNotFound,
      errorFileNotOpen,
      errorBufferTooSmall,
      errorSeekError,
      errorSectionNotFound,
      errorKeyNotFound,
      errorEndOfFile,
      errorUnknownError,
    };
    inline error_t getError(void) {
      return _error;
    };
    inline void clearError(void) {
      _error = errorNoError;
    };
    String printError(uint8_t e);

    // Validate
    bool validate(char* buffer, size_t len);

    // Get value from the file, but split into many short tasks. Return
    // value: false means continue, true means stop. Call getError() to
    // find out if any error
    bool getValue(const char* section, const char* key, char* buffer, size_t len, IniFileState &state);
    bool getValue(const char* section, const char* key, char* buffer, size_t len);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, char *value, size_t vlen);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, bool& b);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, int& val);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, uint16_t& val);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, long& val);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, unsigned long& val);
    bool getValue(const char* section, const char* key, char* buffer, size_t len, float& val);

    // Utility function to read a line from a file
    error_t readLine(char *buffer, size_t len, uint32_t &pos);
    bool isCommentChar(char c);
    char* skipWhiteSpace(char* str);
    void removeTrailingWhiteSpace(char* str);

    // Protected Members
  protected:
    bool findSection(const char* section, char* buffer, size_t len, IniFileState &state);
    bool findKey(const char* section, const char* key, char* buffer, size_t len, char** keyptr, IniFileState &state);

    // Private Members
  private:
    error_t _error;
    File _file;
};
#endif


