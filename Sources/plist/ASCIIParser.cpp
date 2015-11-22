/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <plist/ASCIIParser.h>
#include <plist/ASCIIPListLexer.h>
#include <plist/ASCIIPListParser.h>

#include <plist/Objects.h>

using plist::ASCIIParser;
using plist::Object;
using plist::String;
using plist::Integer;
using plist::Real;
using plist::Boolean;
using plist::Data;
using plist::Array;
using plist::Dictionary;

ASCIIParser::ASCIIParser()
{
}

typedef enum _ASCIIParseState {
    kASCIIParsePList = 0,
    kASCIIParseKeyValSeparator,
    kASCIIParseEntrySeparator,

    kASCIIInvalid = -1
} ASCIIParseState;

#if 0
#define ASCIIDebug(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#else
#define ASCIIDebug(...)
#endif

static inline uint8_t
hex_to_bin_digit(char ch)
{
    if (ch >= 'a' && ch <= 'f')
        return (ch - 'a') + 10;
    else if (ch >= 'A' && ch <= 'F')
        return (ch - 'A') + 10;
    else if (ch >= '0' && ch <= '9')
        return (ch - '0');
    else
        return 0;
}

static inline uint8_t
hex_to_bin(char const *ascii)
{
    return (hex_to_bin_digit(ascii[0]) << 4) | hex_to_bin_digit(ascii[1]);
}

static bool
ASCIIParserParse(ASCIIPListParserContext *context, ASCIIPListLexer *lexer)
{
    int token;
    ASCIIParseState state = kASCIIParsePList;

    for (;;) {
        token = ASCIIPListLexerReadToken(lexer);
        if (token < 0) {
            if (token == kASCIIPListLexerEndOfFile && ASCIIPListParserIsDone(context)) {
                /* success */
                return true;
            } else if (token == kASCIIPListLexerEndOfFile) {
                ASCIIPListParserAbort(context, "Encountered premature EOF");
                return false;
            } else if (token == kASCIIPListLexerInvalidToken) {
                ASCIIPListParserAbort(context, "Encountered invalid token");
                return false;
            } else if (token == kASCIIPListLexerUnterminatedLongComment) {
                ASCIIPListParserAbort(context, "Encountered unterminated long comment");
                return false;
            } else if (token == kASCIIPListLexerUnterminatedUnquotedString) {
                ASCIIPListParserAbort(context, "Encountered unterminated unquoted string");
                return false;
            } else if (token == kASCIIPListLexerUnterminatedQuotedString) {
                ASCIIPListParserAbort(context, "Encountered unterminated quoted string");
                return false;
            } else if (token == kASCIIPListLexerUnterminatedData) {
                ASCIIPListParserAbort(context, "Encountered unterminated data");
                return false;
            } else {
                ASCIIPListParserAbort(context, "Encountered unrecognized token error code");
                return false;
            }
        }

        /* Ignore comments */
        if (token == kASCIIPListLexerTokenInlineComment ||
            token == kASCIIPListLexerTokenLongComment)
            continue;

        switch (state) {
            case kASCIIParsePList:
                if (token != kASCIIPListLexerTokenUnquotedString &&
                    token != kASCIIPListLexerTokenQuotedString &&
                    token != kASCIIPListLexerTokenData &&
                    token != kASCIIPListLexerTokenNumber &&
                    token != kASCIIPListLexerTokenHexNumber &&
                    token != kASCIIPListLexerTokenBoolFalse &&
                    token != kASCIIPListLexerTokenBoolTrue &&
                    token != kASCIIPListLexerTokenDictionaryStart &&
                    token != kASCIIPListLexerTokenArrayStart &&
                    token != kASCIIPListLexerTokenDictionaryEnd &&
                    token != kASCIIPListLexerTokenArrayEnd) {
                    ASCIIPListParserAbort(context, "Encountered unexpected token code");
                    return false;
                }

                if (ASCIIPListParserIsDone(context)) {
                    ASCIIPListParserAbort(context, "Encountered token when finished.");
                    return false;
                }

                if (token != kASCIIPListLexerTokenDictionaryStart &&
                    token != kASCIIPListLexerTokenDictionaryEnd &&
                    token != kASCIIPListLexerTokenArrayStart &&
                    token != kASCIIPListLexerTokenArrayEnd) {
                    /* Read all non-container tokens */
                    bool topLevel     = ASCIIPListParserGetLevel(context) == 0;
                    bool isDictionary = ASCIIPListParserIsDictionary(context);

                    if (isDictionary || token == kASCIIPListLexerTokenUnquotedString || token == kASCIIPListLexerTokenQuotedString) {
                        if (isDictionary && token == kASCIIPListLexerTokenData) {
                            ASCIIPListParserAbort(context, "Data cannot be dictionary key");
                            return false;
                        }

                        char *contents = ASCIIPListCopyUnquotedString(lexer, '?');
                        String *string = String::New(std::string(contents));
                        free(contents);

                        if (string == NULL) {
                            ASCIIPListParserAbort(context, "OOM when copying string");
                            return false;
                        }

                        /* Container context */
                        if (isDictionary) {
                            ASCIIDebug("Storing string %s as key", string->value().c_str());
                            if (!ASCIIPListParserStoreKey(context, string)) {
                                string->release();
                                return false;
                            }
                        } else {
                            ASCIIDebug("Storing string %s", string->value().c_str());
                            if (!ASCIIPListParserStoreValue(context, string)) {
                                string->release();
                                return false;
                            }
                        }

                        string->release();
                    } else if (token == kASCIIPListLexerTokenNumber || token == kASCIIPListLexerTokenHexNumber) {
                        char *contents = ASCIIPListCopyUnquotedString(lexer, '?');
                        bool isReal = strchr(contents, '.') != NULL;

                        if (isReal) {
                            Real *real = Real::New(atof(contents));
                            free(contents);
                            if (real == NULL) {
                                ASCIIPListParserAbort(context, "OOM when creating real");
                                return false;
                            }

                            ASCIIDebug("Storing real");
                            if (!ASCIIPListParserStoreValue(context, real)) {
                                real->release();
                                return false;
                            }

                            real->release();
                        } else {
                            Integer *integer = Integer::New(atol(contents));
                            free(contents);
                            if (integer == NULL) {
                                ASCIIPListParserAbort(context, "OOM when creating integer");
                                return false;
                            }

                            ASCIIDebug("Storing integer");
                            if (!ASCIIPListParserStoreValue(context, integer)) {
                                integer->release();
                                return false;
                            }

                            integer->release();
                        }
                    } else if (token == kASCIIPListLexerTokenBoolTrue || token == kASCIIPListLexerTokenBoolFalse) {
                        bool value = (token == kASCIIPListLexerTokenBoolTrue);
                        Boolean *boolean = Boolean::New(value);
                        if (boolean == NULL) {
                            ASCIIPListParserAbort(context, "OOM when creating boolean");
                            return false;
                        }

                        ASCIIDebug("Storing boolean");
                        if (!ASCIIPListParserStoreValue(context, boolean)) {
                            boolean->release();
                            return false;
                        }

                        boolean->release();
                    } else if (token == kASCIIPListLexerTokenData) {
                        char   *contents = ASCIIPListCopyData(lexer);
                        size_t  alength  = strlen(contents);
                        size_t  length   = alength / 2;
                        auto    bytes    = std::vector<uint8_t>(length, 0);

                        for (size_t n = 0; n < alength; n += 2) {
                            bytes[n >> 1] = hex_to_bin(contents + n);
                        }

                        Data *data = Data::New(bytes);
                        free(contents);

                        if (data == NULL) {
                            ASCIIPListParserAbort(context, "OOM when copying data");
                            return false;
                        }

                        ASCIIDebug("Storing string as data");
                        if (!ASCIIPListParserStoreValue(context, data)) {
                            data->release();
                            return false;
                        }

                        data->release();
                    }

                    if (topLevel) {
                        if (!ASCIIPListParserFinish(context)) {
                            return false;
                        }
                        state = kASCIIParsePList;
                    } else {
                        if (isDictionary) {
                            state = kASCIIParseKeyValSeparator;
                        } else {
                            state = kASCIIParseEntrySeparator;
                        }
                    }
                } else if (token == kASCIIPListLexerTokenDictionaryStart) {
                    ASCIIDebug("Starting dictionary");
                    if (!ASCIIPListParserDictionaryBegin(context)) {
                        return false;
                    }
                    ASCIIPListParserIncrementLevel(context);
                    state = kASCIIParsePList;
                } else if (token == kASCIIPListLexerTokenArrayStart) {
                    ASCIIDebug("Starting array");
                    if (!ASCIIPListParserArrayBegin(context)) {
                        return false;
                    }
                    ASCIIPListParserIncrementLevel(context);
                    state = kASCIIParsePList;
                } else if (token == kASCIIPListLexerTokenDictionaryEnd) {
                    ASCIIDebug("Ending dictionary");
                    if (!ASCIIPListParserDictionaryEnd(context)) {
                        return false;
                    }
                    ASCIIPListParserDecrementLevel(context);
                    if (ASCIIPListParserGetLevel(context)) {
                        state = kASCIIParseEntrySeparator;
                    } else {
                        if (!ASCIIPListParserFinish(context)) {
                            return false;
                        }
                        state = kASCIIParsePList;
                    }
                } else if (token == kASCIIPListLexerTokenArrayEnd) {
                    ASCIIDebug("Ending array");
                    if (!ASCIIPListParserArrayEnd(context)) {
                        return false;
                    }
                    ASCIIPListParserDecrementLevel(context);
                    if (ASCIIPListParserGetLevel(context)) {
                        state = kASCIIParseEntrySeparator;
                    } else {
                        if (!ASCIIPListParserFinish(context)) {
                            return false;
                        }
                        state = kASCIIParsePList;
                    }
                }
                break;

            case kASCIIParseKeyValSeparator:
                if (token != kASCIIPListLexerTokenDictionaryKeyValSeparator) {
                    ASCIIPListParserAbort(context, "Expected key-value separator; found something else");
                    return false;
                }
                ASCIIDebug("Found keyval separator");
                state = kASCIIParsePList;
                break;

            case kASCIIParseEntrySeparator:
                if (token != ';' && token != ',' &&
                    /*
                     * Arrays do not require a final separator. Dictionaries do.
                     */
                    token != kASCIIPListLexerTokenArrayEnd) {
                    ASCIIPListParserAbort(context, "Expected entry separator or array end; found something else");
                    return false;
                }

                if (ASCIIPListParserIsDictionary(context) && token != ';') {
                    ASCIIPListParserAbort(context, "Expected ';'");
                    return false;
                }

                if (ASCIIPListParserIsArray(context) &&
                    token != ',' &&
                    token != kASCIIPListLexerTokenArrayEnd) {
                    ASCIIPListParserAbort(context, "Expected ',' or ')'");
                    return false;
                }

                if (token == kASCIIPListLexerTokenArrayEnd) {
                    if (ASCIIPListParserIsDictionary(context)) {
                        ASCIIPListParserAbort(context, NULL);
                        return false;
                    }
                    ASCIIDebug("Found array end");
                    if (!ASCIIPListParserArrayEnd(context)) {
                        return false;
                    }

                    ASCIIPListParserDecrementLevel(context);
                    if (ASCIIPListParserGetLevel(context)) {
                        state = kASCIIParseEntrySeparator;
                    } else {
                        if (!ASCIIPListParserFinish(context)) {
                            return false;
                        }
                        state = kASCIIParsePList;
                    }
                } else {
                    ASCIIDebug("Found entry separator");
                    state = kASCIIParsePList;
                }
                break;
            default:
                ASCIIPListParserAbort(context, "Unexpected state");
                return false;
        }
    }
}

Object *ASCIIParser::
parse(std::string const &path, error_function const &error)
{
    std::FILE *fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr)
        return nullptr;

    Object *object = parse(fp, error);

    std::fclose(fp);
    return object;
}

Object *ASCIIParser::
parse(std::FILE *fp, error_function const &error)
{
    Object             *root = nullptr;
    bool                success;
    ASCIIPListParserContext  context;
    ASCIIPListLexer     lexer;

    if (fseek(fp, 0, SEEK_END) != 0) {
        return nullptr;
    }

    long fsize = ftell(fp);
    if (fsize == -1) {
        return nullptr;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        return nullptr;
    }

    char *data = (char *)malloc(fsize + 1);
    if (data == NULL) {
        return nullptr;
    }

    if (fread(data, fsize, 1, fp) != 1) {
        return nullptr;
    }

    ASCIIPListParserContextInit(&context);
    ASCIIPListLexerInit(&lexer, data, fsize, kASCIIPListLexerStyleASCII);

    /* Parse contents. */
    success = ASCIIParserParse(&context, &lexer);
    if (success) {
        root = ASCIIPListParserCopyRoot(&context);
    } else {
        error(0, 0, context.error);
    }

    ASCIIPListParserContextFree(&context);
    free(data);
    return root;
}

