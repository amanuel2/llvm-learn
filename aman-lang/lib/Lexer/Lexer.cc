#include "amanlang/Lexer/Lexer.h"

namespace amanlang {

///////////////////////////////////////////////////////////////////////////
#pragma mark - KeywordFilter
///////////////////////////////////////////////////////////////////////////

void KeywordFilter::addKeyword (Kwargs&& kwargs) {
    HashTable.insert ({ kwargs.Name, kwargs.Kind });
}

void KeywordFilter::addKeywords () {
#define KEYWORD(NAME, FLAG) \
    addKeyword ({ llvm::StringRef (#NAME), tok::kw_##NAME });
#include "amanlang/Basic/TokenKinds.def"
#undef KEYWORD
}
} // namespace amanlang

///////////////////////////////////////////////////////////////////////////
#pragma mark - Lexer Utilities
///////////////////////////////////////////////////////////////////////////

namespace charinfo {
constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isASCII (char Ch) {
    return static_cast<unsigned char> (Ch) <= 127;
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isVerticalWhitespace (char Ch) {
    return isASCII (Ch) && (Ch == '\r' || Ch == '\n');
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isHorizontalWhitespace (char Ch) {
    return isASCII (Ch) && (Ch == ' ' || Ch == '\t' || Ch == '\f' || Ch == '\v');
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isWhitespace (char Ch) {
    return isHorizontalWhitespace (Ch) || isVerticalWhitespace (Ch);
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isDigit (char Ch) {
    return isASCII (Ch) && Ch >= '0' && Ch <= '9';
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isHexDigit (char Ch) {
    return isASCII (Ch) && (isDigit (Ch) || (Ch >= 'A' && Ch <= 'F'));
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isIdentifierHead (char Ch) {
    return isASCII (Ch) &&
    (Ch == '_' || (Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z'));
}

constexpr LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isIdentifierBody (char Ch) {
    return isIdentifierHead (Ch) || isDigit (Ch);
}
} // namespace charinfo


///////////////////////////////////////////////////////////////////////////
#pragma mark - Lexer Utilities
///////////////////////////////////////////////////////////////////////////

namespace amanlang {
void Lexer::formToken (Token& Result, const char* TokEnd, tok::TokenKind Kind) {
    size_t TokLen = TokEnd - Ptr;
    Result.Ptr    = Ptr;
    Result.Len    = TokLen;
    Result.Kind   = Kind;
    Ptr           = TokEnd;
}

void Lexer::number (Token& Result) {
    const char* End     = Ptr + 1;
    tok::TokenKind Kind = tok::unknown;
    bool IsHex          = false;
    // check if hex
    while (*End) {
        if (!charinfo::isHexDigit (*End))
            break;
        if (!charinfo::isDigit (*End))
            IsHex = true;
        ++End;
    }
    switch (*End) {
    case 'H': /* hex number */
        Kind = tok::integer_literal;
        ++End;
        break;
    default: /* decimal number */
        if (IsHex)
            Diag.report (getLoc (), diag::err_hex_digit_in_decimal);
        Kind = tok::integer_literal;
        break;
    }
    formToken (Result, End, Kind);
}
void Lexer::identifier (Token& Result) {
    const char *Start = Ptr, *End = Ptr + 1;
    while (charinfo::isIdentifierBody (*End))
        ++End;
    llvm::StringRef Name (Start, End - Start);
    formToken (Result, End, KwFilter.getKeyword ({ Name, tok::identifier }));
}
void Lexer::string (Token& Result) {
    const char *Start = Ptr, *End = Ptr + 1;
    while (*End && *End != *Start && !charinfo::isVerticalWhitespace (*End))
        ++End;
    if (charinfo::isVerticalWhitespace (*End)) {
        Diag.report (getLoc (), diag::err_unterminated_char_or_string);
    }
    formToken (Result, End + 1, tok::string_literal);
}
void Lexer::comment () {
    const char* End = Ptr + 2;
    unsigned Level  = 1;
    while (*End && Level) {

        if (*End == '(' && *(End + 1) == '*') {
            End += 2;
            Level++;
        }

        else if (*End == '*' && *(End + 1) == ')') {
            End += 2;
            Level--;
        } else
            ++End;
    }
    if (!*End) {
        Diag.report (getLoc (), diag::err_unterminated_block_comment);
    }
    Ptr = End;
}

void Lexer::next (Token& Result) {
    while (*Ptr && charinfo::isWhitespace (*Ptr)) {
        ++Ptr;
    }
    if (!*Ptr) {
        Result.setKind (tok::eof);
        return;
    }
    if (charinfo::isIdentifierHead (*Ptr)) {
        identifier (Result);
        return;
    } else if (charinfo::isDigit (*Ptr)) {
        number (Result);
        return;
    } else if (*Ptr == '"' || *Ptr == '\'') {
        string (Result);
        return;
    } else {
        switch (*Ptr) {
        case '=': formToken (Result, Ptr + 1, tok::equal); break;
        case '#': formToken (Result, Ptr + 1, tok::hash); break;
        case '+': formToken (Result, Ptr + 1, tok::plus); break;
        case '-': formToken (Result, Ptr + 1, tok::minus); break;
        case '*': formToken (Result, Ptr + 1, tok::star); break;
        case '/': formToken (Result, Ptr + 1, tok::slash); break;
        case ',': formToken (Result, Ptr + 1, tok::comma); break;
        case '.': formToken (Result, Ptr + 1, tok::period); break;
        case ';': formToken (Result, Ptr + 1, tok::semi); break;
        case ')': formToken (Result, Ptr + 1, tok::r_paren); break;
        case '(':
            if (*(Ptr + 1) == '*') {
                comment ();
                next (Result);
            } else
                formToken (Result, Ptr + 1, tok::l_paren);
            break;
        case ':':
            if (*(Ptr + 1) == '=')
                formToken (Result, Ptr + 2, tok::colonequal);
            else
                formToken (Result, Ptr + 1, tok::colon);
            break;
        case '<':
            if (*(Ptr + 1) == '=')
                formToken (Result, Ptr + 2, tok::lessequal);
            else
                formToken (Result, Ptr + 1, tok::less);
            break;
        case '>':
            if (*(Ptr + 1) == '=')
                formToken (Result, Ptr + 2, tok::greaterequal);
            else
                formToken (Result, Ptr + 1, tok::greater);
            break;
        default: Result.setKind (tok::unknown);
        }
        return;
    }
}
} // namespace amanlang
