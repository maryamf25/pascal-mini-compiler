#ifndef INTEGRATED_SYMBOL_TABLE_H
#define INTEGRATED_SYMBOL_TABLE_H

#include "common.h"

enum class SymbolKind {
    VARIABLE,
    CONSTANT,
    FUNCTION,
    PROCEDURE,
    PARAMETER,
    ARRAY
};

enum class SymbolType {
    INTEGER,
    REAL,
    BOOLEAN,
    CHAR,
    VOID_TYPE,
    UNKNOWN
};

struct SymbolEntry {
    int id;
    string name;
    SymbolKind kind;
    SymbolType type;
    int scopeLevel;
    int line;
    int column;
    int offset;
    shared_ptr<SymbolEntry> next;
};

class SymbolTable {
private:
    static const int TABLE_SIZE = 211;
    vector<shared_ptr<SymbolEntry>> slots;
    int nextOffset = 0;

    int hashValue(const string& text) const {
        unsigned long h = 5381;
        for (char c : text) {
            h = ((h << 5) + h) + static_cast<unsigned char>(c);
        }
        return static_cast<int>(h % TABLE_SIZE);
    }

public:
    int scopeLevel;
    SymbolTable* parent;

    SymbolTable(int level, SymbolTable* parentScope) : slots(TABLE_SIZE), scopeLevel(level), parent(parentScope) {}

    shared_ptr<SymbolEntry> lookupCurrent(const string& name) const {
        int h = hashValue(name);
        for (auto e = slots[h]; e != nullptr; e = e->next) {
            if (e->name == name) return e;
        }
        return nullptr;
    }

    shared_ptr<SymbolEntry> lookup(const string& name) const {
        const SymbolTable* table = this;
        while (table != nullptr) {
            auto found = table->lookupCurrent(name);
            if (found != nullptr) return found;
            table = table->parent;
        }
        return nullptr;
    }

    shared_ptr<SymbolEntry> insert(int id, const string& name, SymbolKind kind,
                                   SymbolType type, int line, int column) {
        if (lookupCurrent(name) != nullptr) return nullptr;
        int h = hashValue(name);
        auto entry = make_shared<SymbolEntry>();
        entry->id = id;
        entry->name = name;
        entry->kind = kind;
        entry->type = type;
        entry->scopeLevel = scopeLevel;
        entry->line = line;
        entry->column = column;
        entry->offset = nextOffset++;
        entry->next = slots[h];
        slots[h] = entry;
        return entry;
    }

    vector<shared_ptr<SymbolEntry>> entries() const {
        vector<shared_ptr<SymbolEntry>> result;
        for (auto head : slots) {
            for (auto e = head; e != nullptr; e = e->next) result.push_back(e);
        }
        sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a->id < b->id;
        });
        return result;
    }

    static string kindName(SymbolKind kind) {
        switch (kind) {
            case SymbolKind::VARIABLE: return "variable";
            case SymbolKind::CONSTANT: return "constant";
            case SymbolKind::FUNCTION: return "function";
            case SymbolKind::PROCEDURE: return "procedure";
            case SymbolKind::PARAMETER: return "parameter";
            case SymbolKind::ARRAY: return "array";
        }
        return "unknown";
    }

    static string typeName(SymbolType type) {
        switch (type) {
            case SymbolType::INTEGER: return "integer";
            case SymbolType::REAL: return "real";
            case SymbolType::BOOLEAN: return "boolean";
            case SymbolType::CHAR: return "char";
            case SymbolType::VOID_TYPE: return "void";
            case SymbolType::UNKNOWN: return "unknown";
        }
        return "unknown";
    }

    void print(ostream& out) const {
        out << "+-----+----------------+-----------+---------+-------+------+--------+\n";
        out << "| ID  | Name           | Kind      | Type    | Scope | Line | Offset |\n";
        out << "+-----+----------------+-----------+---------+-------+------+--------+\n";
        auto all = entries();
        if (all.empty()) {
            out << "| --  | <empty>        | --        | --      | "
                << setw(5) << scopeLevel << " | --   | --     |\n";
        } else {
            for (auto e : all) {
                out << "| " << setw(3) << e->id << " "
                    << "| " << left << setw(14) << e->name << right << " "
                    << "| " << left << setw(9) << kindName(e->kind) << right << " "
                    << "| " << left << setw(7) << typeName(e->type) << right << " "
                    << "| " << setw(5) << e->scopeLevel << " "
                    << "| " << setw(4) << e->line << " "
                    << "| " << setw(6) << e->offset << " |\n";
            }
        }
        out << "+-----+----------------+-----------+---------+-------+------+--------+\n";
    }
};

class SymbolTableManager {
private:
    vector<unique_ptr<SymbolTable>> ownedScopes;
    SymbolTable* currentScope = nullptr;
    int nextId = 1;

public:
    void reset() {
        ownedScopes.clear();
        currentScope = nullptr;
        nextId = 1;
    }

    SymbolTable* beginScope(ostream& out) {
        int level = currentScope == nullptr ? 0 : currentScope->scopeLevel + 1;
        ownedScopes.push_back(make_unique<SymbolTable>(level, currentScope));
        currentScope = ownedScopes.back().get();
        out << "[Scope " << level << "] Enter\n";
        return currentScope;
    }

    void endScope(ostream& out) {
        if (currentScope == nullptr) return;
        out << "[Scope " << currentScope->scopeLevel << "] Exit, dump:\n";
        currentScope->print(out);
        currentScope = currentScope->parent;
    }

    shared_ptr<SymbolEntry> insert(const Token& token, SymbolKind kind,
                                   SymbolType type, ErrorHandler& errors, ostream& out) {
        if (currentScope == nullptr) beginScope(out);
        auto entry = currentScope->insert(nextId, token.lexeme, kind, type, token.line, token.column);
        if (entry == nullptr) {
            errors.add("Semantic", token.line, token.column,
                       "duplicate declaration '" + token.lexeme + "'");
            out << "ERROR line " << token.line << ": duplicate declaration '" << token.lexeme << "'\n";
            return nullptr;
        }
        ++nextId;
        out << "[Scope " << entry->scopeLevel << "] insert " << token.lexeme
            << " : " << SymbolTable::kindName(kind) << ", "
            << SymbolTable::typeName(type) << ", line " << token.line << "\n";
        return entry;
    }

    shared_ptr<SymbolEntry> lookup(const Token& token, ErrorHandler& errors, ostream& out) const {
        if (currentScope == nullptr) {
            errors.add("Semantic", token.line, token.column,
                       "undeclared variable '" + token.lexeme + "'");
            return nullptr;
        }
        auto entry = currentScope->lookup(token.lexeme);
        if (entry == nullptr) {
            errors.add("Semantic", token.line, token.column,
                       "undeclared variable '" + token.lexeme + "'");
            out << "ERROR line " << token.line << ": undeclared variable '" << token.lexeme << "'\n";
        } else {
            out << "[Scope " << currentScope->scopeLevel << "] lookup " << token.lexeme
                << " -> found at scope " << entry->scopeLevel << "\n";
        }
        return entry;
    }

    SymbolTable* current() const {
        return currentScope;
    }
};

class SemanticAnalyzer {
private:
    vector<Token> tokens;
    size_t pos = 0;
    SymbolTableManager symbols;

    Token current() const {
        if (pos < tokens.size()) return tokens[pos];
        return {ENDMARKER, ENDMARKER, 0, 0};
    }

    bool check(const string& type) const {
        return current().type == type;
    }

    bool match(const string& type) {
        if (check(type)) {
            ++pos;
            return true;
        }
        return false;
    }

    SymbolType tokenToType(const Token& token) const {
        if (token.type == "integer") return SymbolType::INTEGER;
        if (token.type == "real") return SymbolType::REAL;
        return SymbolType::UNKNOWN;
    }

    bool isExpressionStop(const string& type) const {
        return type == ";" || type == "end" || type == "THEN" ||
               type == "DO" || type == "ELSE" || type == ENDMARKER;
    }

    void parseProgramHeader(ErrorHandler& errors, ostream& out) {
        if (match("PROGRAM")) {
            if (check("ID")) {
                Token name = current();
                ++pos;
                symbols.beginScope(out);
                symbols.insert(name, SymbolKind::FUNCTION, SymbolType::VOID_TYPE, errors, out);
            }
            while (!check(";") && !check(ENDMARKER)) ++pos;
            match(";");
        } else {
            symbols.beginScope(out);
        }
    }

    void parseDeclaration(ErrorHandler& errors, ostream& out) {
        vector<Token> names;
        while (check("ID")) {
            names.push_back(current());
            ++pos;
            if (!match(",")) break;
        }
        if (!match(":")) return;
        if (!(check("integer") || check("real"))) return;

        Token typeToken = current();
        SymbolType type = tokenToType(typeToken);
        ++pos;
        for (const Token& name : names) {
            symbols.insert(name, SymbolKind::VARIABLE, type, errors, out);
        }
        match(";");
    }

    void parseExpression(ErrorHandler& errors, ostream& out) {
        while (!isExpressionStop(current().type)) {
            if (check("ID")) symbols.lookup(current(), errors, out);
            ++pos;
        }
    }

    void parseStatementOrExpression(ErrorHandler& errors, ostream& out) {
        if (check("ID")) {
            Token name = current();
            ++pos;
            if (match(":=")) {
                symbols.lookup(name, errors, out);
                parseExpression(errors, out);
                match(";");
            } else if (check(":")) {
                errors.add("Semantic", name.line, name.column,
                           "declaration appears outside a var section for '" + name.lexeme + "'");
                while (!check(";") && !check(ENDMARKER)) ++pos;
                match(";");
            } else {
                symbols.lookup(name, errors, out);
            }
        } else {
            ++pos;
        }
    }

public:
    bool analyze(const vector<Token>& input, ErrorHandler& errors, ostream& out) {
        tokens = input;
        pos = 0;
        parseProgramHeader(errors, out);

        while (!check(ENDMARKER)) {
            if (match("VAR")) {
                while (check("ID")) parseDeclaration(errors, out);
            } else if (match("begin")) {
                symbols.beginScope(out);
            } else if (match("end")) {
                symbols.endScope(out);
                match(".");
                match(";");
            } else {
                parseStatementOrExpression(errors, out);
            }
        }

        while (symbols.current() != nullptr) symbols.endScope(out);
        return !errors.hasErrors();
    }
};

#endif
