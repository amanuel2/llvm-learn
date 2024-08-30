#include "Lexer.h"


namespace charinfo {
LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isWhiteSpace(char c) {
  return c == ' ' || c == '\t' || c == '\f' || c == '\v' || c == '\r' ||
         c == '\n';
}

LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

LLVM_READNONE LLVM_ATTRIBUTE_ALWAYS_INLINE bool isLetter(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}

} // namespace charinfo

void Lexer::next(Token &Tok) {
    while (BufferPtr && *BufferPtr && charinfo::isWhiteSpace(*BufferPtr)) {
        ++BufferPtr;
    }

    if (!BufferPtr || !*BufferPtr) {
        Tok.Kind = Token::eoi;
        return;
    }

    if (charinfo::isLetter(*BufferPtr)) {
        const char *end = BufferPtr+1;
        while (charinfo::isLetter(*end)) ++end;
        llvm::StringRef Name(BufferPtr, end - BufferPtr);
        Token::TokenKind kind =
            Name == "with" ? Token::KW_with : Token::ident;
        formToken(Tok, end, kind);
    } else if (charinfo::isDigit(*BufferPtr)) {
      const char *end = BufferPtr + 1;
      while (charinfo::isDigit(*end)) ++end;
      formToken(Tok, end, Token::number);
    } else {
      switch(*BufferPtr) {
        case '+': formToken(Tok, BufferPtr+1, Token::plus); break;
        case '-': formToken(Tok, BufferPtr+1, Token::minus); break;
        case '*': formToken(Tok, BufferPtr+1, Token::star); break;
        case '/': formToken(Tok, BufferPtr+1, Token::slash); break;
        case '(': formToken(Tok, BufferPtr+1, Token::Token::l_paren); break;
        case ')': formToken(Tok, BufferPtr+1, Token::Token::r_paren); break;
        case ':': formToken(Tok, BufferPtr+1, Token::Token::colon); break;
        case ',': formToken(Tok, BufferPtr+1, Token::Token::comma); break;
        default: formToken(Tok, BufferPtr+1, Token::unknown);
      }
    }
}


void Lexer::formToken(Token &Result, const char *TokEnd, Token::TokenKind Kind) {
  Result.Kind = Kind;
  Result.Lexeme = llvm::StringRef(BufferPtr, TokEnd - BufferPtr);
  BufferPtr = TokEnd;
}