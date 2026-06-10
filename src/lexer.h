#ifndef LEXER_H
#define LEXER_H
#include <cstring>

#include "common.h"

class Lexer {
private:
    ifstream fileStream;
    long fileLen = 0;
    long absPos = 0;
    static const int CHUNK = 4096;
    char buf[2][CHUNK];
    long blockStart[2] = {-1, -1};
    int bufLenArr[2] = {0, 0};
    int line = 1;
    int column = 1;
    ErrorHandler* errors = nullptr;
    ostream* traceStream = nullptr;

    void ensureLoaded(long index) {
        if (index >= fileLen) return;
        if (blockStart[0] <= index && index < blockStart[0] + bufLenArr[0]) return;
        if (blockStart[1] <= index && index < blockStart[1] + bufLenArr[1]) return;
        long start = (index / CHUNK) * CHUNK;
        fileStream.seekg(start, ios::beg);
        fileStream.read(buf[1], CHUNK);
        bufLenArr[1] = static_cast<int>(fileStream.gcount());
        blockStart[1] = start;
        swap(blockStart[0], blockStart[1]);
        swap(bufLenArr[0], bufLenArr[1]);
        char tmp[CHUNK];
        memcpy(tmp, buf[0], CHUNK);
        memcpy(buf[0], buf[1], CHUNK);
        memcpy(buf[1], tmp, CHUNK);
    }

    char peek(int offset = 0) {
        long p = absPos + offset;
        if (p >= fileLen) return '\0';
        ensureLoaded(p);
        if (blockStart[0] <= p && p < blockStart[0] + bufLenArr[0]) return buf[0][p - blockStart[0]];
        if (blockStart[1] <= p && p < blockStart[1] + bufLenArr[1]) return buf[1][p - blockStart[1]];
        return '\0';
    }

    char advance() {
        char c = peek();
        if (c == '\0') return c;
        ++absPos;
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        return c;
    }

    void addToken(vector<Token>& tokens, const string& type, const string& lexeme,
                  int startLine, int startColumn) {
        tokens.push_back({type, lexeme, startLine, startColumn});
        if (traceStream != nullptr) {
            (*traceStream) << "addToken: type='" << type << "' lexeme='" << lexeme
                           << "' line=" << startLine << " col=" << startColumn << "\n";
        }
    }

    void skipComment() {
        int startLine = line;
        int startColumn = column;
        if (traceStream) (*traceStream) << "skipComment at " << startLine << "," << startColumn << "\n";
        advance();
        while (peek() != '\0' && peek() != '}') advance();
        if (peek() == '}') {
            advance();
            if (traceStream) (*traceStream) << "comment closed\n";
        } else if (errors != nullptr) {
            errors->add("Lexical", startLine, startColumn, "unterminated comment");
        }
    }

    void scanIdentifier(vector<Token>& tokens) {
        int startLine = line;
        int startColumn = column;
        string lexeme;
        if (traceStream) (*traceStream) << "scanIdentifier start at " << startLine << "," << startColumn << "\n";
        while (isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
            lexeme += advance();
        }

        string lower = lowerText(lexeme);
        if (lower == "program") addToken(tokens, "PROGRAM", lexeme, startLine, startColumn);
        else if (lower == "var") addToken(tokens, "VAR", lexeme, startLine, startColumn);
        else if (lower == "array") addToken(tokens, "ARRAY", lexeme, startLine, startColumn);
        else if (lower == "of") addToken(tokens, "OF", lexeme, startLine, startColumn);
        else if (lower == "integer") addToken(tokens, "integer", lexeme, startLine, startColumn);
        else if (lower == "real") addToken(tokens, "real", lexeme, startLine, startColumn);
        else if (lower == "function") addToken(tokens, "FUNCTION", lexeme, startLine, startColumn);
        else if (lower == "procedure") addToken(tokens, "PROCEDURE", lexeme, startLine, startColumn);
        else if (lower == "begin") addToken(tokens, "begin", lexeme, startLine, startColumn);
        else if (lower == "end") addToken(tokens, "end", lexeme, startLine, startColumn);
        else if (lower == "if") addToken(tokens, "IF", lexeme, startLine, startColumn);
        else if (lower == "then") addToken(tokens, "THEN", lexeme, startLine, startColumn);
        else if (lower == "else") addToken(tokens, "ELSE", lexeme, startLine, startColumn);
        else if (lower == "while") addToken(tokens, "WHILE", lexeme, startLine, startColumn);
        else if (lower == "do") addToken(tokens, "DO", lexeme, startLine, startColumn);
        else if (lower == "not") addToken(tokens, "not", lexeme, startLine, startColumn);
        else if (lower == "or") addToken(tokens, "addop", lexeme, startLine, startColumn);
        else if (lower == "div" || lower == "mod" || lower == "and") {
            addToken(tokens, "mulop", lexeme, startLine, startColumn);
        } else {
            addToken(tokens, "ID", lexeme, startLine, startColumn);
        }
    }

    void scanNumber(vector<Token>& tokens) {
        int startLine = line;
        int startColumn = column;
        string lexeme;
        if (traceStream) (*traceStream) << "scanNumber start at " << startLine << "," << startColumn << "\n";

        while (isdigit(static_cast<unsigned char>(peek()))) lexeme += advance();
        if (peek() == '.' && peek(1) != '.') {
            lexeme += advance();
            while (isdigit(static_cast<unsigned char>(peek()))) lexeme += advance();
        }
        addToken(tokens, "num", lexeme, startLine, startColumn);
    }

    void scanOperator(vector<Token>& tokens) {
        int startLine = line;
        int startColumn = column;
        char c = advance();
        if (traceStream) (*traceStream) << "scanOperator got '" << c << "' at " << startLine << "," << startColumn << "\n";

        if (c == ':' && peek() == '=') {
            advance();
            addToken(tokens, ":=", ":=", startLine, startColumn);
        } else if (c == ':' || c == ';' || c == ',' || c == '(' || c == ')' ||
                   c == '[' || c == ']') {
            addToken(tokens, string(1, c), string(1, c), startLine, startColumn);
        } else if (c == '.' && peek() == '.') {
            advance();
            addToken(tokens, "..", "..", startLine, startColumn);
        } else if (c == '.') {
            addToken(tokens, ".", ".", startLine, startColumn);
        } else if (c == '+' || c == '-') {
            addToken(tokens, "addop", string(1, c), startLine, startColumn);
        } else if (c == '*' || c == '/') {
            addToken(tokens, "mulop", string(1, c), startLine, startColumn);
        } else if (c == '<' || c == '>' || c == '=') {
            string lexeme(1, c);
            if ((c == '<' || c == '>') && (peek() == '=' || peek() == '>')) {
                lexeme += advance();
            }
            addToken(tokens, "relop", lexeme, startLine, startColumn);
        } else if (errors != nullptr) {
            errors->add("Lexical", startLine, startColumn,
                        "unknown character '" + string(1, c) + "'");
        }
    }

public:
    bool init(const string& filename, ErrorHandler& handler) {
        errors = &handler;
        if (fileStream.is_open()) fileStream.close();
        fileStream.open(filename, ios::in | ios::binary);
        if (!fileStream.is_open()) {
            handler.add("Input", 0, 0, "cannot open file '" + filename + "'");
            return false;
        }

        fileStream.seekg(0, ios::end);
        fileLen = static_cast<long>(fileStream.tellg());
        fileStream.seekg(0, ios::beg);

        blockStart[0] = 0;
        fileStream.read(buf[0], CHUNK);
        bufLenArr[0] = static_cast<int>(fileStream.gcount());
        blockStart[1] = -1;
        bufLenArr[1] = 0;

        absPos = 0;
        line = 1;
        column = 1;
        return true;
    }

    Token getNextToken() {
        vector<Token> temp;
        while (peek() != '\0') {
            if (isspace(static_cast<unsigned char>(peek()))) {
                if (traceStream) (*traceStream) << "whitespace '" << peek() << "' at " << line << "," << column << "\n";
                advance();
            }
            else if (peek() == '{') skipComment();
            else if (isalpha(static_cast<unsigned char>(peek())) || peek() == '_') {
                scanIdentifier(temp);
                return temp.front();
            }
            else if (isdigit(static_cast<unsigned char>(peek()))) {
                scanNumber(temp);
                return temp.front();
            }
            else {
                scanOperator(temp);
                if (!temp.empty()) return temp.front();
            }
        }
        return {ENDMARKER, ENDMARKER, line, column};
    }

    vector<Token> tokenizeFile(const string& filename, ErrorHandler& handler) {
        if (!init(filename, handler)) return {};
        traceStream = nullptr;
        vector<Token> tokens;
        Token t;
        do {
            t = getNextToken();
            tokens.push_back(t);
        } while (t.type != ENDMARKER);
        return tokens;
    }

    void setTraceStream(ostream& out) { traceStream = &out; }
};

#endif