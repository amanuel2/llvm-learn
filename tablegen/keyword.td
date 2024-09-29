class Flag<string name, int val> {
    string Name = name;
    int Val = val;
}

class Token {
    string Name;
}


// Defined in .def file
/*
 From TokenKinds.def:
 #ifndef TOK
#define TOK(ID)
#endif
#ifndef PUNCTUATOR
#define PUNCTUATOR(ID, SP) TOK(ID)
#endif
#ifndef KEYWORD
#define KEYWORD(ID, FLAG) TOK(kw_ ## ID)
#endif
*/
class Tok<string name> : Token {
    let Name = name;
}

class Punctuator<string name, string spelling> : Token {
    let Name = name;
    string Spelling = spelling;
}

class Keyword<string name, list<Flag> flags> : Token {
    let Name = name;
    list<Flag> Flags = flags;
}


// Filterer
class TokenFilter<list<Token> tokens> {
    string FunctionName;
    list<Token> Tokens = tokens;
}