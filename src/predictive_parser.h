#ifndef PREDICTIVE_PARSER_H
#define PREDICTIVE_PARSER_H

#include "common.h"
#include "symbol_table.h"

class PredictiveParser {
private:
    using Production = pair<string, vector<string>>;
    vector<Production> grammar;
    set<string> nonterminals;
    set<string> terminals;
    map<string, set<string>> firstSet;
    map<string, set<string>> followSet;
    map<pair<string, string>, int> table;
    SymbolTableManager symbols;
    bool declarationMode = false;
    vector<Token> pendingDeclarationNames;
    SymbolKind pendingDeclarationKind = SymbolKind::VARIABLE;
    SymbolType pendingDeclarationType = SymbolType::UNKNOWN;
    string previousMatchedTerminal;

    bool prepared = false;

    bool isNonterminal(const string& symbol) const {
        return nonterminals.count(symbol) > 0;
    }

    SymbolType tokenToType(const Token& token) const {
        if (token.type == "integer") return SymbolType::INTEGER;
        if (token.type == "real") return SymbolType::REAL;
        return SymbolType::UNKNOWN;
    }

    enum class NameMode {
        LOOKUP,
        DECL_VARIABLE_LIST,
        DECL_PARAMETER_LIST,
        DECL_FUNCTION_NAME,
        DECL_PROCEDURE_NAME,
        DECL_PROGRAM_NAME,
        PROGRAM_HEADER_LIST
    };

    static bool isActionSymbol(const string& symbol) {
        return !symbol.empty() && symbol[0] == '@';
    }

    vector<string> productionExpansion(int prod) const {
        const string& lhs = grammar[prod].first;
        const vector<string>& rhs = grammar[prod].second;

        if (lhs == "program") {
            return {"@program_scope_begin", "PROGRAM", "@declare_program_name", "ID", "(", "@program_header_list_begin", "identifier_list", "@program_header_list_end", ")", ";", "declarations", "subprogram_declarations", "compound_statement"};
        }
        if (lhs == "declarations" && rhs.size() == 6 && rhs[0] == "VAR") {
            return {"VAR", "@decl_var_start", "identifier_list", "@decl_end", ":", "type", ";", "declarations_p"};
        }
        if (lhs == "declarations_p" && rhs.size() == 5 && rhs[0] == "identifier_list") {
            return {"@decl_var_start", "identifier_list", "@decl_end", ":", "type", ";", "declarations_p"};
        }
        if (lhs == "subprogram_declarations" && rhs.size() == 3) {
            return {"subprogram_declaration", ";", "subprogram_declarations"};
        }
        if (lhs == "subprogram_declaration") {
            return {"subprogram_head", "declarations", "compound_statement", "@scope_end"};
        }
        if (lhs == "subprogram_head" && rhs.size() == 6 && rhs[0] == "FUNCTION") {
            return {"FUNCTION", "@declare_function_name", "ID", "@subprogram_scope_begin", "arguments", ":", "standard_type", ";"};
        }
        if (lhs == "subprogram_head" && rhs.size() == 4 && rhs[0] == "PROCEDURE") {
            return {"PROCEDURE", "@declare_procedure_name", "ID", "@subprogram_scope_begin", "arguments", ";"};
        }
        if (lhs == "parameter_list" && rhs.size() == 6 && rhs[0] == "identifier_list") {
            return {"@decl_param_start", "identifier_list", "@decl_end", ":", "type", "parameter_list_p"};
        }
        if (lhs == "parameter_list_p" && rhs.size() == 6 && rhs[0] == ";") {
            return {";", "@decl_param_start", "identifier_list", "@decl_end", ":", "type", "parameter_list_p"};
        }
        if (lhs == "compound_statement") {
            return {"@scope_begin", "begin", "block_declarations", "optional_statements", "end", "@scope_end"};
        }
        if (lhs == "block_declarations" && rhs.size() == 5 && rhs[0] == "VAR") {
            return {"VAR", "@decl_var_start", "identifier_list", "@decl_end", ":", "type", ";"};
        }
        return rhs;
    }

    void pushProduction(stack<string>& parseStack, int prod) {
        vector<string> expansion = productionExpansion(prod);
        for (int i = static_cast<int>(expansion.size()) - 1; i >= 0; --i) {
            if (expansion[i] == EPSILON) continue;
            parseStack.push(expansion[i]);
        }
    }

    void executeAction(const string& action, vector<NameMode>& nameModes, ErrorHandler& errors, ostream& out) {
        if (action == "@program_scope_begin" || action == "@subprogram_scope_begin" || action == "@scope_begin") {
            symbols.beginScope(out);
            return;
        }
        if (action == "@scope_end") {
            symbols.endScope(out);
            return;
        }
        if (action == "@decl_var_start") {
            pendingDeclarationNames.clear();
            pendingDeclarationKind = SymbolKind::VARIABLE;
            pendingDeclarationType = SymbolType::UNKNOWN;
            nameModes.push_back(NameMode::DECL_VARIABLE_LIST);
            return;
        }
        if (action == "@decl_param_start") {
            pendingDeclarationNames.clear();
            pendingDeclarationKind = SymbolKind::PARAMETER;
            pendingDeclarationType = SymbolType::UNKNOWN;
            nameModes.push_back(NameMode::DECL_PARAMETER_LIST);
            return;
        }
        if (action == "@declare_program_name") {
            nameModes.push_back(NameMode::DECL_PROGRAM_NAME);
            return;
        }
        if (action == "@program_header_list_begin") {
            nameModes.push_back(NameMode::PROGRAM_HEADER_LIST);
            return;
        }
        if (action == "@program_header_list_end") {
            if (!nameModes.empty() && nameModes.back() == NameMode::PROGRAM_HEADER_LIST) {
                nameModes.pop_back();
            }
            return;
        }
        if (action == "@declare_function_name") {
            nameModes.push_back(NameMode::DECL_FUNCTION_NAME);
            return;
        }
        if (action == "@declare_procedure_name") {
            nameModes.push_back(NameMode::DECL_PROCEDURE_NAME);
            return;
        }
        if (action == "@decl_end") {
            if (!pendingDeclarationNames.empty()) {
                insertPendingDeclarations(errors, out);
            }
            if (!nameModes.empty() && (nameModes.back() == NameMode::DECL_VARIABLE_LIST || nameModes.back() == NameMode::DECL_PARAMETER_LIST)) {
                nameModes.pop_back();
            }
            return;
        }
    }

    void handleIdentifier(const Token& token, vector<NameMode>& nameModes, ErrorHandler& errors, ostream& out) {
        if (!nameModes.empty()) {
            NameMode mode = nameModes.back();
            if (mode == NameMode::PROGRAM_HEADER_LIST) {
                return;
            }
            if (mode == NameMode::DECL_VARIABLE_LIST) {
                pendingDeclarationNames.push_back(token);
                return;
            }
            if (mode == NameMode::DECL_PARAMETER_LIST) {
                pendingDeclarationNames.push_back(token);
                return;
            }
            if (mode == NameMode::DECL_PROGRAM_NAME) {
                symbols.insert(token, SymbolKind::FUNCTION, SymbolType::VOID_TYPE, errors, out);
                nameModes.pop_back();
                return;
            }
            if (mode == NameMode::DECL_FUNCTION_NAME) {
                symbols.insert(token, SymbolKind::FUNCTION, SymbolType::VOID_TYPE, errors, out);
                nameModes.pop_back();
                return;
            }
            if (mode == NameMode::DECL_PROCEDURE_NAME) {
                symbols.insert(token, SymbolKind::PROCEDURE, SymbolType::VOID_TYPE, errors, out);
                nameModes.pop_back();
                return;
            }
        }

        symbols.lookup(token, errors, out);
    }

    void handleMatchedToken(const Token& token, vector<NameMode>& nameModes, ErrorHandler& errors, ostream& out) {
        if (token.type == "ID") {
            handleIdentifier(token, nameModes, errors, out);
        } else if ((token.type == "integer" || token.type == "real") && !pendingDeclarationNames.empty()) {
            pendingDeclarationType = tokenToType(token);
            insertPendingDeclarations(errors, out);
        }
    }

    void insertPendingDeclarations(ErrorHandler& errors, ostream& out) {
        if (pendingDeclarationNames.empty()) return;
        for (const Token& name : pendingDeclarationNames) {
            symbols.insert(name, pendingDeclarationKind, pendingDeclarationType, errors, out);
        }
        pendingDeclarationNames.clear();
        pendingDeclarationKind = SymbolKind::VARIABLE;
        pendingDeclarationType = SymbolType::UNKNOWN;
    }

    void defineGrammar() {
        grammar = {
            {"program", {"PROGRAM", "ID", "(", "identifier_list", ")", ";", "declarations", "subprogram_declarations", "compound_statement"}},
            {"identifier_list", {"ID", "identifier_list_p"}},
            {"identifier_list_p", {",", "ID", "identifier_list_p"}},
            {"identifier_list_p", {EPSILON}},
            {"declarations", {"VAR", "identifier_list", ":", "type", ";", "declarations_p"}},
            {"declarations", {EPSILON}},
            {"declarations_p", {"identifier_list", ":", "type", ";", "declarations_p"}},
            {"declarations_p", {EPSILON}},
            {"type", {"standard_type"}},
            {"type", {"ARRAY", "[", "num", "..", "num", "]", "OF", "standard_type"}},
            {"standard_type", {"integer"}},
            {"standard_type", {"real"}},
            {"subprogram_declarations", {"subprogram_declaration", ";", "subprogram_declarations"}},
            {"subprogram_declarations", {EPSILON}},
            {"subprogram_declaration", {"subprogram_head", "declarations", "compound_statement"}},
            {"subprogram_head", {"FUNCTION", "ID", "arguments", ":", "standard_type", ";"}},
            {"subprogram_head", {"PROCEDURE", "ID", "arguments", ";"}},
            {"arguments", {"(", "parameter_list", ")"}},
            {"arguments", {EPSILON}},
            {"parameter_list", {"identifier_list", ":", "type", "parameter_list_p"}},
            {"parameter_list_p", {";", "identifier_list", ":", "type", "parameter_list_p"}},
            {"parameter_list_p", {EPSILON}},
            {"compound_statement", {"begin", "block_declarations", "optional_statements", "end"}},
            {"block_declarations", {"VAR", "identifier_list", ":", "type", ";"}},
            {"block_declarations", {EPSILON}},
            {"optional_statements", {"statement_list"}},
            {"optional_statements", {EPSILON}},
            {"statement_list", {"statement", "statement_list_p"}},
            {"statement_list_p", {";", "statement_list_p2"}},
            {"statement_list_p", {EPSILON}},
            {"statement_list_p2", {"statement", "statement_list_p"}},
            {"statement_list_p2", {EPSILON}},
            {"statement", {"ID", "statement_tail"}},
            {"statement", {"compound_statement"}},
            {"statement", {"IF", "expression", "THEN", "statement", "ELSE", "statement"}},
            {"statement", {"WHILE", "expression", "DO", "statement"}},
            {"statement_tail", {"variable_prime", ":=", "expression"}},
            {"statement_tail", {"procedure_statement_prime"}},
            {"variable_prime", {"[", "expression", "]"}},
            {"variable_prime", {EPSILON}},
            {"procedure_statement_prime", {"(", "expression_list", ")"}},
            {"procedure_statement_prime", {EPSILON}},
            {"expression_list", {"expression", "expression_list_p"}},
            {"expression_list_p", {",", "expression", "expression_list_p"}},
            {"expression_list_p", {EPSILON}},
            {"expression", {"simple_expression", "expression_prime"}},
            {"expression_prime", {"relop", "simple_expression"}},
            {"expression_prime", {EPSILON}},
            {"simple_expression", {"term", "simple_expression_p"}},
            {"simple_expression", {"sign", "term", "simple_expression_p"}},
            {"simple_expression_p", {"addop", "term", "simple_expression_p"}},
            {"simple_expression_p", {EPSILON}},
            {"term", {"factor", "term_p"}},
            {"term_p", {"mulop", "factor", "term_p"}},
            {"term_p", {EPSILON}},
            {"factor", {"ID", "factor_id_tail"}},
            {"factor", {"num"}},
            {"factor", {"(", "expression", ")"}},
            {"factor", {"not", "factor"}},
            {"factor_id_tail", {"(", "expression_list", ")"}},
            {"factor_id_tail", {EPSILON}},
            {"sign", {"addop"}}
        };
    }

    void collectSymbols() {
        nonterminals.clear();
        terminals.clear();
        firstSet.clear();
        followSet.clear();

        for (const auto& p : grammar) {
            nonterminals.insert(p.first);
            firstSet[p.first];
            followSet[p.first];
        }

        for (const auto& p : grammar) {
            for (const string& s : p.second) {
                if (s != EPSILON && !isNonterminal(s)) {
                    terminals.insert(s);
                    firstSet[s].insert(s);
                }
            }
        }
        terminals.insert(ENDMARKER);
        firstSet[ENDMARKER].insert(ENDMARKER);
    }

    set<string> firstOfSequence(const vector<string>& seq) const {
        set<string> result;
        bool allNullable = true;
        for (const string& symbol : seq) {
            if (symbol == EPSILON) continue;
            auto it = firstSet.find(symbol);
            set<string> fs = (it == firstSet.end()) ? set<string>{symbol} : it->second;
            for (const string& x : fs) {
                if (x != EPSILON) result.insert(x);
            }
            if (fs.count(EPSILON) == 0) {
                allNullable = false;
                break;
            }
        }
        if (allNullable) result.insert(EPSILON);
        return result;
    }

    void computeFirst() {
        bool changed;
        do {
            changed = false;
            for (const auto& p : grammar) {
                const string& lhs = p.first;
                const vector<string>& rhs = p.second;
                if (rhs.size() == 1 && rhs[0] == EPSILON) {
                    changed |= firstSet[lhs].insert(EPSILON).second;
                    continue;
                }

                bool allNullable = true;
                for (const string& symbol : rhs) {
                    if (symbol == EPSILON) continue;
                    set<string> fs = firstSet[symbol];
                    for (const string& x : fs) {
                        if (x != EPSILON) changed |= firstSet[lhs].insert(x).second;
                    }
                    if (fs.count(EPSILON) == 0) {
                        allNullable = false;
                        break;
                    }
                }
                if (allNullable) changed |= firstSet[lhs].insert(EPSILON).second;
            }
        } while (changed);
    }

    void computeFollow() {
        followSet[grammar[0].first].insert(ENDMARKER);
        bool changed;
        do {
            changed = false;
            for (const auto& p : grammar) {
                set<string> trailer = followSet[p.first];
                for (int i = static_cast<int>(p.second.size()) - 1; i >= 0; --i) {
                    const string& symbol = p.second[i];
                    if (isNonterminal(symbol)) {
                        for (const string& x : trailer) changed |= followSet[symbol].insert(x).second;
                        if (firstSet[symbol].count(EPSILON)) {
                            set<string> nextTrailer = trailer;
                            for (const string& x : firstSet[symbol]) {
                                if (x != EPSILON) nextTrailer.insert(x);
                            }
                            trailer = nextTrailer;
                        } else {
                            trailer = firstSet[symbol];
                        }
                    } else {
                        trailer = {symbol};
                    }
                }
            }
        } while (changed);
    }

    void buildTable() {
        table.clear();
        for (int i = 0; i < static_cast<int>(grammar.size()); ++i) {
            set<string> fs = firstOfSequence(grammar[i].second);
            for (const string& t : fs) {
                if (t != EPSILON) table[{grammar[i].first, t}] = i;
            }
            if (fs.count(EPSILON)) {
                for (const string& t : followSet[grammar[i].first]) {
                    table[{grammar[i].first, t}] = i;
                }
            }
        }
    }

    string productionText(int index) const {
        string text = grammar[index].first + " -> ";
        for (const string& s : grammar[index].second) text += s + " ";
        return text;
    }

public:
    void prepare() {
        if (prepared) return;
        defineGrammar();
        collectSymbols();
        computeFirst();
        computeFollow();
        buildTable();
        prepared = true;
    }

    void printGrammar(ostream& out) {
        defineGrammar();
        for (size_t i = 0; i < grammar.size(); ++i) {
            out << to_string(i) << ": " << productionText(static_cast<int>(i)) << "\n";
        }
    }

    void printParsingTable(ostream& out) {
        prepare();
        out << "LL(1) Parsing Table Entries:\n";
        for (const auto& kv : table) {
            out << "[" << kv.first.first << ", " << kv.first.second << "] -> prod " << kv.second << " : " << productionText(kv.second) << "\n";
        }
    }

    bool parse(Lexer& lexObj, ErrorHandler& errors, ostream& out) {
        prepare();
        symbols.reset();
        vector<NameMode> nameModes;

        Token curToken = lexObj.getNextToken();
        Token nextToken = curToken.type != ENDMARKER ? lexObj.getNextToken() : Token{ENDMARKER, ENDMARKER, 0, 0};

        auto advance = [&]() {
            curToken = nextToken;
            if (curToken.type != ENDMARKER) {
                nextToken = lexObj.getNextToken();
            }
        };

        stack<string> parseStack;
        parseStack.push(ENDMARKER);
        parseStack.push(grammar[0].first);

        int step = 1;
        bool ok = true;

        out << left << setw(6) << "Step" << setw(38) << "Stack"
            << setw(14) << "Input" << "Action\n";
        out << string(90, '-') << "\n";

        while (!parseStack.empty()) {
            while (curToken.type == "." && nextToken.type == ENDMARKER) {
                advance();
            }

            string top = parseStack.top();
            string cur = curToken.type;
            string next = nextToken.type;

            if (top == EPSILON) {
                parseStack.pop();
                continue;
            }

            if (isActionSymbol(top)) {
                parseStack.pop();
                executeAction(top, nameModes, errors, out);
                continue;
            }

            stack<string> temp = parseStack;
            vector<string> items;
            while (!temp.empty()) {
                items.push_back(temp.top());
                temp.pop();
            }
            reverse(items.begin(), items.end());
            string stackText;
            for (const string& item : items) stackText += item + " ";
            if (stackText.size() > 36) stackText = stackText.substr(0, 33) + "...";

            out << left << setw(6) << step++ << setw(38) << stackText << setw(14) << cur;

            if (top == ENDMARKER && cur == ENDMARKER) {
                out << "ACCEPT\n";
                break;
            }

            if (!isNonterminal(top)) {
                if (top == cur) {
                    out << "match " << cur << "\n";
                    handleMatchedToken(curToken, nameModes, errors, out);
                    parseStack.pop();
                    advance();
                } else {
                    out << "ERROR\n";
                    Token got = curToken;
                    errors.add("Syntax", got.line, got.column,
                               "predictive parser expected '" + top + "' but got '" + cur + "'");
                    set<string> sync = followSet[top];
                    sync.insert(";"); sync.insert("end"); sync.insert(ENDMARKER);
                    while (curToken.type != ENDMARKER && sync.count(curToken.type) == 0) advance();
                    parseStack.pop();
                    ok = false; 
                    continue;
                }
            } else {
                if (top == "declarations_p" && cur == "ID") {
                    if (next == ":" || next == ",") {
                        out << "declarations_p -> identifier_list : type ; declarations_p\n";
                        parseStack.pop();
                        parseStack.push("declarations_p");
                        parseStack.push(";");
                        parseStack.push("type");
                        parseStack.push(":");
                        parseStack.push("@decl_end");
                        parseStack.push("identifier_list");
                        parseStack.push("@decl_var_start");
                        continue;
                    }
                    out << "declarations_p -> eps\n";
                    parseStack.pop();
                    continue;
                }

                auto it = table.find({top, cur});
                if (it == table.end()) {
                    out << "ERROR: no table entry\n";
                    Token got = curToken;
                    errors.add("Syntax", got.line, got.column,
                               "no LL(1) table entry for (" + top + ", " + cur + ")");
                    set<string> sync = followSet[top];
                    sync.insert(";"); sync.insert("end"); sync.insert(ENDMARKER);
                    while (curToken.type != ENDMARKER && sync.count(curToken.type) == 0) {
                        advance();
                    }
                    parseStack.pop();
                    ok = false;
                    continue;
                }

                int prod = it->second;
                out << productionText(prod) << "\n";
                parseStack.pop();
                pushProduction(parseStack, prod);
            }
        }

        while (!nameModes.empty()) nameModes.pop_back();
        while (symbols.current() != nullptr) symbols.endScope(out);

        out << (ok ? "PREDICTIVE RESULT: ACCEPTED\n" : "PREDICTIVE RESULT: REJECTED\n");
        return ok;
    }

    void printFirstFollow(ostream& out) {
        prepare();
        out << "FIRST/FOLLOW SETS\n";
        for (const string& nt : nonterminals) {
            out << left << setw(28) << nt << "FIRST={ ";
            for (const string& x : firstSet[nt]) out << x << " ";
            out << "} FOLLOW={ ";
            for (const string& x : followSet[nt]) out << x << " ";
            out << "}\n";
        }
    }
};

#endif
