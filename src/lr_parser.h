#ifndef LR_PARSER_H
#define LR_PARSER_H

#include "common.h"
#include "symbol_table.h"

class LRParser {
private:
    using Production = pair<string, vector<string>>;
    vector<Production> grammar; 
    set<string> nonterminals;
    set<string> terminals;
    map<string, set<string>> firstSet;
    map<string, set<string>> followSet;

    struct SemanticValue {
        Token token{ENDMARKER, ENDMARKER, 0, 0};
        vector<Token> names;
        SymbolType type = SymbolType::UNKNOWN;
        bool hasToken = false;
        bool hasNames = false;
        bool hasType = false;
    };

    SymbolTableManager symbols;

    SymbolType tokenToType(const Token& token) const {
        if (token.type == "integer") return SymbolType::INTEGER;
        if (token.type == "real") return SymbolType::REAL;
        return SymbolType::UNKNOWN;
    }

    bool isTypeToken(const string& type) const {
        return type == "integer" || type == "real";
    }

    SemanticValue makeTokenValue(const Token& token) const {
        SemanticValue value;
        value.token = token;
        value.hasToken = true;
        if (isTypeToken(token.type)) {
            value.type = tokenToType(token);
            value.hasType = true;
        }
        return value;
    }

    void resetSemanticState() {
        symbols.reset();
    }

    void beginScope(ostream& out) {
        symbols.beginScope(out);
    }

    void endScope(ostream& out) {
        if (symbols.current() != nullptr) symbols.endScope(out);
    }

    void reduceSemanticAction(int prodIndex, vector<SemanticValue>& semanticStack, ErrorHandler& errors, ostream& out) {
        const auto& lhs = grammar[prodIndex].first;
        const auto& rhs = grammar[prodIndex].second;
        vector<SemanticValue> children(rhs.size());
        for (int i = static_cast<int>(rhs.size()) - 1; i >= 0; --i) {
            if (!semanticStack.empty()) {
                children[i] = semanticStack.back();
                semanticStack.pop_back();
            }
        }

        SemanticValue result;

        if (lhs == "identifier_list" && rhs.size() == 2) {
            if (children[0].hasToken) result.names.push_back(children[0].token);
            if (children[1].hasNames) result.names.insert(result.names.end(), children[1].names.begin(), children[1].names.end());
            result.hasNames = true;
        } else if (lhs == "identifier_list_p" && rhs.size() == 3) {
            if (children[1].hasToken) result.names.push_back(children[1].token);
            if (children[2].hasNames) result.names.insert(result.names.end(), children[2].names.begin(), children[2].names.end());
            result.hasNames = true;
        } else if (lhs == "identifier_list_p" && rhs.empty()) {
            result.hasNames = true;
        } else if (lhs == "standard_type" && rhs.size() == 1 && children[0].hasToken) {
            result.type = tokenToType(children[0].token);
            result.hasType = true;
        } else if (lhs == "type" && rhs.size() == 1 && children[0].hasType) {
            result.type = children[0].type;
            result.hasType = true;
        } else if (lhs == "type" && rhs.size() == 8 && children[7].hasType) {
            result.type = children[7].type;
            result.hasType = true;
        } else if (lhs == "parameter_list" && rhs.size() == 6) {
            if (children[0].hasNames && children[4].hasType) {
                for (const Token& name : children[0].names) {
                    symbols.insert(name, SymbolKind::PARAMETER, children[4].type, errors, out);
                }
            }
        } else if (lhs == "parameter_list_p" && rhs.size() == 6) {
            if (children[1].hasNames && children[4].hasType) {
                for (const Token& name : children[1].names) {
                    symbols.insert(name, SymbolKind::PARAMETER, children[4].type, errors, out);
                }
            }
        } else if (lhs == "declarations" && rhs.size() == 6) {
            if (children[1].hasNames && children[3].hasType) {
                for (const Token& name : children[1].names) {
                    symbols.insert(name, SymbolKind::VARIABLE, children[3].type, errors, out);
                }
            }
        } else if (lhs == "declarations_p" && rhs.size() == 5) {
            if (children[0].hasNames && children[2].hasType) {
                for (const Token& name : children[0].names) {
                    symbols.insert(name, SymbolKind::VARIABLE, children[2].type, errors, out);
                }
            }
        } else if (lhs == "block_declarations" && rhs.size() == 5) {
            if (children[1].hasNames && children[3].hasType) {
                for (const Token& name : children[1].names) {
                    symbols.insert(name, SymbolKind::VARIABLE, children[3].type, errors, out);
                }
            }
        } else if (lhs == "subprogram_head" && rhs.size() == 6 && children[1].hasToken) {
            symbols.insert(children[1].token, SymbolKind::FUNCTION, SymbolType::VOID_TYPE, errors, out);
        } else if (lhs == "subprogram_head" && rhs.size() == 4 && children[1].hasToken) {
            symbols.insert(children[1].token, SymbolKind::PROCEDURE, SymbolType::VOID_TYPE, errors, out);
        } else if (lhs == "program" && rhs.size() == 9 && children[1].hasToken) {
            symbols.insert(children[1].token, SymbolKind::FUNCTION, SymbolType::VOID_TYPE, errors, out);
        } else if (lhs == "statement" && rhs.size() == 2 && children[0].hasToken) {
            symbols.lookup(children[0].token, errors, out);
        } else if (lhs == "factor" && rhs.size() == 2 && children[0].hasToken) {
            symbols.lookup(children[0].token, errors, out);
        } else if (lhs == "compound_statement" && rhs.size() == 4) {
            endScope(out);
        } else if (lhs == "subprogram_declaration" && rhs.size() == 3) {
            endScope(out);
        } else if (lhs == "program" && rhs.size() == 9) {
            endScope(out);
        }

        semanticStack.push_back(result);
    }

    struct Item {
        int prod;
        int dot;
        string lookahead;
        bool operator<(Item const& o) const {
            if (prod != o.prod) return prod < o.prod;
            if (dot != o.dot) return dot < o.dot;
            return lookahead < o.lookahead;
        }
        bool operator==(Item const& o) const { return prod == o.prod && dot == o.dot && lookahead == o.lookahead; }
    };

    using ItemSet = set<Item>;

    vector<ItemSet> states;
    map<pair<int, string>, int> gotoTable;
    map<pair<int, string>, pair<string,int>> actionTable;

    bool isNonterminal(const string& s) const { return nonterminals.count(s) > 0; }

    void defineGrammar() {
        grammar = {
            {"program", {"PROGRAM", "ID", "(", "identifier_list", ")", ";", "declarations", "subprogram_declarations", "compound_statement"}},
            {"identifier_list", {"ID", "identifier_list_p"}},
            {"identifier_list_p", {",", "ID", "identifier_list_p"}},
            {"identifier_list_p", {}},
            {"declarations", {"VAR", "identifier_list", ":", "type", ";", "declarations_p"}},
            {"declarations", {}},
            {"declarations_p", {"identifier_list", ":", "type", ";", "declarations_p"}},
            {"declarations_p", {}},
            {"type", {"standard_type"}},
            {"type", {"ARRAY", "[", "num", "..", "num", "]", "OF", "standard_type"}},
            {"standard_type", {"integer"}},
            {"standard_type", {"real"}},
                {"subprogram_declarations", {"subprogram_declaration", ";", "subprogram_declarations"}},
                {"subprogram_declarations", {}},
            {"subprogram_declaration", {"subprogram_head", "declarations", "compound_statement"}},
            {"subprogram_head", {"FUNCTION", "ID", "arguments", ":", "standard_type", ";"}},
            {"subprogram_head", {"PROCEDURE", "ID", "arguments", ";"}},
            {"arguments", {"(", "parameter_list", ")"}},
            {"arguments", {}},
            {"parameter_list", {"identifier_list", ":", "type", "parameter_list_p"}},
            {"parameter_list_p", {";", "identifier_list", ":", "type", "parameter_list_p"}},
            {"parameter_list_p", {}},
            {"compound_statement", {"begin", "block_declarations", "optional_statements", "end"}},
            {"block_declarations", {"VAR", "identifier_list", ":", "type", ";"}},
            {"block_declarations", {}},
            {"optional_statements", {"statement_list"}},
            {"optional_statements", {}},
            {"statement_list", {"statement", "statement_list_p"}},
            {"statement_list_p", {";", "statement_list_p2"}},
            {"statement_list_p", {}},
            {"statement_list_p2", {"statement", "statement_list_p"}},
            {"statement_list_p2", {}},
            {"statement", {"ID", "statement_tail"}},
            {"statement", {"compound_statement"}},
            {"statement", {"IF", "expression", "THEN", "statement", "ELSE", "statement"}},
            {"statement", {"WHILE", "expression", "DO", "statement"}},
            {"statement_tail", {"variable_prime", ":=", "expression"}},
            {"statement_tail", {"procedure_statement_prime"}},
            {"variable_prime", {"[", "expression", "]"}},
            {"variable_prime", {}},
            {"procedure_statement_prime", {"(", "expression_list", ")"}},
            {"procedure_statement_prime", {}},
            {"expression_list", {"expression", "expression_list_p"}},
            {"expression_list_p", {",", "expression", "expression_list_p"}},
            {"expression_list_p", {}},
            {"expression", {"simple_expression", "expression_prime"}},
            {"expression_prime", {"relop", "simple_expression"}},
            {"expression_prime", {}},
            {"simple_expression", {"term", "simple_expression_p"}},
            {"simple_expression", {"sign", "term", "simple_expression_p"}},
            {"simple_expression_p", {"addop", "term", "simple_expression_p"}},
            {"simple_expression_p", {}},
            {"term", {"factor", "term_p"}},
            {"term_p", {"mulop", "factor", "term_p"}},
            {"term_p", {}},
            {"factor", {"ID", "factor_id_tail"}},
            {"factor", {"num"}},
            {"factor", {"(", "expression", ")"}},
            {"factor", {"not", "factor"}},
            {"factor_id_tail", {"(", "expression_list", ")"}},
            {"factor_id_tail", {}},
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
            for (const string& x : fs) if (x != EPSILON) result.insert(x);
            if (fs.count(EPSILON) == 0) { allNullable = false; break; }
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
                if (rhs.empty()) { changed |= firstSet[lhs].insert(EPSILON).second; continue; }
                bool allNullable = true;
                for (const string& symbol : rhs) {
                    if (symbol == EPSILON) continue;
                    set<string> fs = firstSet.at(symbol);
                    for (const string& x : fs) if (x != EPSILON) changed |= firstSet[lhs].insert(x).second;
                    if (fs.count(EPSILON) == 0) { allNullable = false; break; }
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
                            for (const string& x : firstSet[symbol]) if (x != EPSILON) nextTrailer.insert(x);
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

    ItemSet closure(const ItemSet& I) {
        ItemSet C = I;
        bool changed;
        do {
            changed = false;
            vector<Item> toAdd;
                for (const Item& it : C) {
                    const auto& rhs = grammar[it.prod].second;
                    if (it.dot < static_cast<int>(rhs.size())) {
                        string B = rhs[it.dot];
                        if (B == EPSILON) {
                            Item ni{it.prod, it.dot + 1, it.lookahead};
                            if (C.count(ni) == 0) toAdd.push_back(ni);
                            continue;
                        }
                        if (isNonterminal(B)) {
                        vector<string> beta;
                        for (int k = it.dot + 1; k < static_cast<int>(rhs.size()); ++k) beta.push_back(rhs[k]);
                        beta.push_back(it.lookahead);
                        set<string> lookaheads = firstOfSequence(beta);
                        for (int p = 0; p < static_cast<int>(grammar.size()); ++p) {
                            if (grammar[p].first == B) {
                                for (const string& la : lookaheads) {
                                    if (la == EPSILON) continue;
                                    Item ni{p, 0, la};
                                    if (C.count(ni) == 0) toAdd.push_back(ni);
                                }
                            }
                        }
                    }
                }
            }
            for (const Item& it : toAdd) { changed |= C.insert(it).second; }
        } while (changed);
        return C;
    }

    ItemSet gotoSet(const ItemSet& I, const string& X) {
        ItemSet J;
        for (const Item& it : I) {
            const auto& rhs = grammar[it.prod].second;
            if (it.dot < static_cast<int>(rhs.size()) && rhs[it.dot] == X) {
                J.insert(Item{it.prod, it.dot + 1, it.lookahead});
            }
        }
        return closure(J);
    }

    void buildCanonicalCollection() {
        states.clear();
        gotoTable.clear();
        actionTable.clear();

       
        ItemSet start;
        
        Production aug = {string("S'"), {grammar[0].first}};
        vector<Production> saved = grammar;
        grammar.insert(grammar.begin(), aug);
        collectSymbols(); computeFirst(); computeFollow();

        start.insert(Item{0,0, ENDMARKER});
        states.push_back(closure(start));

        bool added = true;
        while (added) {
            added = false;
            int n = static_cast<int>(states.size());
            for (int i = 0; i < n; ++i) {
                set<string> symbolsAfter;
                for (const Item& it : states[i]) {
                    const auto& rhs = grammar[it.prod].second;
                    if (it.dot < static_cast<int>(rhs.size())) {
                        const string &sym = rhs[it.dot];
                        if (sym == EPSILON) continue;
                        symbolsAfter.insert(sym);
                    }
                }
                for (const string& X : symbolsAfter) {
                    ItemSet g = gotoSet(states[i], X);
                    if (g.empty()) continue;
                    int j = -1;
                    for (int k = 0; k < static_cast<int>(states.size()); ++k) if (states[k] == g) { j = k; break; }
                    if (j == -1) { j = static_cast<int>(states.size()); states.push_back(g); added = true; }
                    gotoTable[{i, X}] = j;
                }
            }
        }

        for (int i = 0; i < static_cast<int>(states.size()); ++i) {
            for (const Item& it : states[i]) {
                const auto& rhs = grammar[it.prod].second;
                if (it.dot < static_cast<int>(rhs.size())) {
                    string a = rhs[it.dot];
                    if (!isNonterminal(a) && a != EPSILON) {
                        auto jt = gotoTable.find({i, a});
                        if (jt != gotoTable.end()) actionTable[{i, a}] = {"s", jt->second};
                    }
                } else {
                    if (it.prod == 0) {
                        actionTable[{i, ENDMARKER}] = {"acc", 0};
                    } else {
                        int prodIndex = it.prod;
                        string la = it.lookahead;
                        auto key = make_pair(i, la);
                        if (actionTable.find(key) == actionTable.end()) {
                            actionTable[key] = {"r", prodIndex};
                        }
                    }
                }
            }
        }

        map<pair<int,string>, pair<string,int>> remapped;
        for (const auto &kv : actionTable) {
            const auto &key = kv.first;
            const auto &val = kv.second;
            if (val.first == "r") {
                int augIndex = val.second;
                if (augIndex > 0) {
                    int origIndex = augIndex - 1;
                    remapped[key] = {"r", origIndex};
                } else {
                }
            } else {
                remapped[key] = val;
            }
        }
        actionTable.swap(remapped);

      
        for (auto &state : states) {
            ItemSet newSet;
            for (const Item &it : state) {
                Item nit = it;
                if (nit.prod > 0) nit.prod = nit.prod - 1;
                if (nit.prod >= 0) newSet.insert(nit);
            }
            state = newSet;
        }

        grammar = saved;
        collectSymbols(); computeFirst(); computeFollow();
    }

    string productionText(int index) const {
        string text = grammar[index].first + " -> ";
        for (const string& s : grammar[index].second) text += s + " ";
        return text;
    }

public:
    LRParser() {
        defineGrammar();
        collectSymbols();
        computeFirst();
        computeFollow();
        buildCanonicalCollection();
    }

    void dumpTables(ostream& out) {
        out << "LR STATES (items):\n";
        for (size_t i = 0; i < states.size(); ++i) {
            out << "State " << i << ":\n";
            for (const Item& it : states[i]) {
                out << "  [" << it.prod << "] ";
                const auto &rhs = grammar[it.prod].second;
                for (int k = 0; k < static_cast<int>(rhs.size()); ++k) {
                    if (k == it.dot) out << ". ";
                    out << rhs[k] << " ";
                }
                if (it.dot >= static_cast<int>(rhs.size())) out << ". ";
                out << " , lookahead=" << it.lookahead << "\n";
            }
            out << "\n";
        }
        out << "LR ACTION entries:\n";
        for (const auto& kv : actionTable) {
            out << "state " << kv.first.first << ", token '" << kv.first.second << "' -> " << kv.second.first;
            if (kv.second.first == "s" || kv.second.first == "r") out << " " << kv.second.second;
            out << "\n";
        }
        out << "\nLR GOTO entries:\n";
        for (const auto& kv : gotoTable) {
            out << "state " << kv.first.first << ", nonterm '" << kv.first.second << "' -> state " << kv.second << "\n";
        }
    }

    bool parse(Lexer& lexObj, ErrorHandler& errors, ostream& out) {
        resetSemanticState();
        
        Token curToken = lexObj.getNextToken();
        Token nextToken = curToken.type != ENDMARKER ? lexObj.getNextToken() : Token{ENDMARKER, ENDMARKER, 0, 0};

        auto advance = [&]() {
            curToken = nextToken;
            if (curToken.type != ENDMARKER) {
                nextToken = lexObj.getNextToken();
            }
        };

        vector<int> stateStack;
        vector<string> symbolStack;
        vector<SemanticValue> semanticStack;
        stateStack.push_back(0);

        int step = 1;
        out << left << setw(6) << "Step" << setw(20) << "States" << setw(20) << "Input" << "Action\n";
        out << string(80, '-') << "\n";
        bool ok = true;

        while (true) {
            while (curToken.type == "." && nextToken.type == ENDMARKER) {
                advance();
            }

            int s = stateStack.back();
            string a = curToken.type;

            string statesText;
            for (int st : stateStack) statesText += to_string(st) + " ";
            string inputText = a;
            if (a != ENDMARKER) inputText += " ...";

            auto actIt = actionTable.find({s,a});
            string actionStr = "";
            if (actIt == actionTable.end()) {
                ok = false;
                actionStr = "ERROR";
                out << left << setw(6) << step++ << setw(20) << statesText << setw(20) << inputText << actionStr << "\n";
                Token got = curToken;
                errors.add("Syntax", got.line, got.column, "no ACTION entry for state " + to_string(s) + " and token '" + a + "'");
                set<string> syncTokens = {";", "end", "ELSE", "THEN", "DO", ENDMARKER};
                bool recovered = false;
                int skipped = 0;
                while (curToken.type != ENDMARKER) {
                    string na = curToken.type;
                    if (actionTable.find({s, na}) != actionTable.end()) { recovered = true; break; }
                    if (syncTokens.count(na)) {
                        if (actionTable.find({s, na}) != actionTable.end()) { recovered = true; break; }
                        advance(); skipped++; break;
                    }
                    advance(); skipped++;
                }
                if (!recovered) { break; }
                out << "[LR panic] skipped " << skipped << " tokens, resuming at '" << curToken.type << "'\n";
                continue;
            }

            auto act = actIt->second;
            if (act.first == "s") {
                actionStr = string("shift ") + to_string(act.second);
                out << left << setw(6) << step++ << setw(20) << statesText << setw(20) << inputText << actionStr << "\n";
                symbolStack.push_back(a);
                stateStack.push_back(act.second);
                semanticStack.push_back(makeTokenValue(curToken));
                if (a == "PROGRAM" || a == "FUNCTION" || a == "PROCEDURE" || a == "begin") {
                    beginScope(out);
                }
                advance();
            } else if (act.first == "r") {
                int prodIndex = act.second;
                actionStr = string("reduce by [") + productionText(prodIndex) + "]";
                out << left << setw(6) << step++ << setw(20) << statesText << setw(20) << inputText << actionStr << "\n";
                const auto& rhs = grammar[prodIndex].second;
                if (!rhs.empty()) {
                    for (int i = 0; i < static_cast<int>(rhs.size()); ++i) {
                        if (!symbolStack.empty()) symbolStack.pop_back();
                        if (!stateStack.empty()) stateStack.pop_back();
                    }
                }
                reduceSemanticAction(prodIndex, semanticStack, errors, out);
                string A = grammar[prodIndex].first;
                symbolStack.push_back(A);
                int t = stateStack.back();
                auto gt = gotoTable.find({t, A});
                if (gt == gotoTable.end()) {
                    errors.add("Syntax", 0,0, "no GOTO entry for state " + to_string(t) + " and nonterminal '" + A + "'");
                    ok = false; break;
                }
                stateStack.push_back(gt->second);
            } else if (act.first == "acc") {
                actionStr = "ACCEPT";
                out << left << setw(6) << step++ << setw(20) << statesText << setw(20) << inputText << actionStr << "\n";
                break;
            } else {
                actionStr = "ERROR";
                out << left << setw(6) << step++ << setw(20) << statesText << setw(20) << inputText << actionStr << "\n";
                Token got = curToken;
                errors.add("Syntax", got.line, got.column, "invalid ACTION entry");
                ok = false; break;
            }
        }

        out << (ok ? "LR PARSER RESULT: ACCEPTED\n" : "LR PARSER RESULT: REJECTED\n");

        while (symbols.current() != nullptr) symbols.endScope(out);

        return ok && !errors.hasErrors();
    }
};

#endif