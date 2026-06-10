#ifndef COMMON_H
#define COMMON_H

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const string EPSILON = "eps";
const string ENDMARKER = "$";

enum class ASTNodeKind {
    PROGRAM,
    VAR_DECL,
    ASSIGN_STMT,
    WHILE_STMT,
    IF_STMT,
    BINARY_EXPR,
    ID_NODE,
    NUM_NODE,
    BLOCK_STMT,
    SUBPROGRAM_DECL,
    EMPTY_NODE
};

struct ASTNode {
    ASTNodeKind kind;
    string value;
    vector<shared_ptr<ASTNode>> children;

    ASTNode(ASTNodeKind k, string v = "") : kind(k), value(v) {}

    void print(ostream& out, int indent = 0) {
        for (int i = 0; i < indent; ++i) out << "  ";
        out << "- [" << nodeKindStr(kind) << "] " << value << "\n";
        for (auto& child : children) {
            if (child) child->print(out, indent + 1);
        }
    }

private:
    string nodeKindStr(ASTNodeKind k) {
        switch (k) {
            case ASTNodeKind::PROGRAM: return "Program";
            case ASTNodeKind::VAR_DECL: return "VarDecl";
            case ASTNodeKind::ASSIGN_STMT: return "Assignment";
            case ASTNodeKind::WHILE_STMT: return "WhileLoop";
            case ASTNodeKind::IF_STMT: return "IfStmt";
            case ASTNodeKind::BINARY_EXPR: return "BinaryExpr";
            case ASTNodeKind::ID_NODE: return "Identifier";
            case ASTNodeKind::NUM_NODE: return "Number";
            case ASTNodeKind::BLOCK_STMT: return "Block";
            case ASTNodeKind::SUBPROGRAM_DECL: return "SubprogramDecl";
            case ASTNodeKind::EMPTY_NODE: return "Empty";
        }
        return "Unknown";
    }
};

struct Token {
    string type;
    string lexeme;
    int line;
    int column;
};

struct CompilerError {
    string phase;
    int line;
    int column;
    string message;
};

class ErrorHandler {
private:
    vector<CompilerError> errors;

public:
    void add(const string& phase, int line, int column, const string& message) {
        errors.push_back({phase, line, column, message});
    }

    bool hasErrors() const {
        return !errors.empty();
    }

    void print(ostream& out) const {
        if (errors.empty()) {
            out << "No errors reported.\n";
            return;
        }

        out << "Error summary:\n";
        for (const auto& e : errors) {
            out << "[" << e.phase << "] line " << e.line << ", column "
                << e.column << ": " << e.message << "\n";
        }
    }
};

inline string lowerText(string s) {
    for (char& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

inline vector<string> tokenTypes(const vector<Token>& tokens) {
    vector<string> result;
    for (const auto& token : tokens) result.push_back(token.type);
    return result;
}

inline void printTokenStream(const vector<Token>& tokens, ostream& out) {
    out << left << setw(14) << "Token" << setw(18) << "Lexeme"
        << setw(8) << "Line" << "Column\n";
    out << string(48, '-') << "\n";
    for (const auto& token : tokens) {
        out << left << setw(14) << token.type << setw(18) << token.lexeme
            << setw(8) << token.line << token.column << "\n";
    }
}

#endif
