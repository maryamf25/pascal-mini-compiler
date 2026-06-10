#ifndef RECURSIVE_DESCENT_PARSER_H
#define RECURSIVE_DESCENT_PARSER_H

#include "common.h"
#include "symbol_table.h"

#ifndef ASTNODEKIND_IF_STMT
#define ASTNODEKIND_IF_STMT
#endif
#ifndef ASTNODEKIND_ARRAY_TYPE
#define ASTNODEKIND_ARRAY_TYPE
#endif
#ifndef ASTNODEKIND_PARAM_DECL
#define ASTNODEKIND_PARAM_DECL
#endif

class RecursiveDescentParser {
private:
    Lexer* lexer = nullptr;
    Token currentToken;
    ErrorHandler* errors = nullptr;
    ostream* out = nullptr;
    int indent = 0;
    SymbolTableManager symbols;
    shared_ptr<ASTNode> astRoot;

public:
    shared_ptr<ASTNode> getASTRoot() const { return astRoot; }
private:

    struct DeclSpec {
        SymbolKind kind = SymbolKind::VARIABLE;
        SymbolType type = SymbolType::UNKNOWN;
        shared_ptr<ASTNode> arrayNode = nullptr;
    };

    string lookahead() const {
        return currentToken.type;
    }

    Token current() const {
        return currentToken;
    }

    SymbolType tokenToType(const Token& token) const {
        if (token.type == "integer") return SymbolType::INTEGER;
        if (token.type == "real") return SymbolType::REAL;
        return SymbolType::UNKNOWN;
    }

    void resetSymbols() {
        symbols.reset();
    }

    string makeVarDeclString(const vector<Token>& names, const DeclSpec& spec) {
        string typeStr;
        if (spec.kind == SymbolKind::ARRAY && spec.arrayNode) {
            typeStr = "array";
        } else {
            typeStr = (spec.type == SymbolType::INTEGER) ? "integer" : 
                      (spec.type == SymbolType::REAL) ? "real" : "unknown";
        }
        string result;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) result += ", ";
            result += names[i].lexeme + " : " + typeStr;
        }
        return result;
    }

    void insertNames(const vector<Token>& names, SymbolKind kind, SymbolType type, shared_ptr<ASTNode> /*arrayInfo*/ = nullptr) {
        if (errors == nullptr || out == nullptr) return;
        for (const Token& name : names) {
            symbols.insert(name, kind, type, *errors, *out);
        }
    }

    DeclSpec parseTypeSpec() {
        DeclSpec spec;
        if (lookahead() == "ARRAY") {
            match("ARRAY");
            match("[");
            Token lower = current();
            match("num");
            match("..");
            Token upper = current();
            match("num");
            match("]");
            match("OF");
            spec.kind = SymbolKind::ARRAY;
            string arrayRange = lower.lexeme + ".." + upper.lexeme;
            spec.arrayNode = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, "ArrayType");
            spec.arrayNode->value = arrayRange;
            if (lookahead() == "integer" || lookahead() == "real") {
                Token typeToken = current();
                spec.type = tokenToType(typeToken);
                match(typeToken.type);
                auto elemNode = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, 
                    (spec.type == SymbolType::INTEGER) ? "integer" : "real");
                spec.arrayNode->children.push_back(elemNode);
            } else {
                reportRuleError("standard_type");
            }
            return spec;
        }

        if (lookahead() == "integer" || lookahead() == "real") {
            Token typeToken = current();
            spec.type = tokenToType(typeToken);
            match(typeToken.type);
            return spec;
        }

        reportRuleError("type");
        return spec;
    }

    bool nt_identifier_list(vector<Token>& names) {
        Token idToken = current();
        if (!match("ID")) return false;
        names.push_back(idToken);
        while (lookahead() == ",") {
            if (!match(",")) return false;
            idToken = current();
            if (!match("ID")) return false;
            names.push_back(idToken);
        }
        return true;
    }

    void traceEnter(const string& name) {
        if (out == nullptr) return;
        for (int i = 0; i < indent; ++i) *out << "  ";
        *out << "-> " << name << "\n";
        ++indent;
    }

    void traceExit(const string& name) {
        if (out == nullptr) return;
        --indent;
        for (int i = 0; i < indent; ++i) *out << "  ";
        *out << "<- " << name << "\n";
    }

    bool match(const string& expected) {
        if (out != nullptr) {
            for (int i = 0; i < indent; ++i) *out << "  ";
            *out << "match(" << expected << ") ";
        }

        if (lookahead() == expected) {
            if (out != nullptr) *out << "OK, got " << current().lexeme << "\n";
            currentToken = lexer->getNextToken();
            return true;
        }

        Token got = current();
        if (out != nullptr) *out << "FAIL, got " << got.type << "\n";
        errors->add("Syntax", got.line, got.column,
                    "expected '" + expected + "' but got '" + got.type + "'");
        return false;
    }

    void reportRuleError(const string& rule) {
        Token got = current();
        errors->add("Syntax", got.line, got.column,
                    "grammar rule '" + rule + "' failed near '" + got.lexeme + "'");
    }

    struct Guard {
        RecursiveDescentParser& parser;
        string name;
        Guard(RecursiveDescentParser& p, const string& n) : parser(p), name(n) {
            parser.traceEnter(name);
        }
        ~Guard() {
            parser.traceExit(name);
        }
    };

    bool nt_program() {
        Guard g(*this, "program");
        if (!match("PROGRAM")) return false;
        Token programName = current();
        if (!match("ID")) return false;
        if (out != nullptr) symbols.beginScope(*out);
        insertNames({programName}, SymbolKind::FUNCTION, SymbolType::VOID_TYPE);
        
        if (!(match("(") && nt_identifier_list() && match(")") && match(";"))) return false;
        
        auto declsNode = nt_declarations();
        auto subprogsNode = nt_subprogram_declarations();
        auto block = nt_compound_statement();
        if (!block) return false;
        
        astRoot = make_shared<ASTNode>(ASTNodeKind::PROGRAM, programName.lexeme);
        if (declsNode) astRoot->children.push_back(declsNode);
        if (subprogsNode) astRoot->children.push_back(subprogsNode);
        if (block) astRoot->children.push_back(block);
        
        return (match(".") || true);
    }

    bool nt_identifier_list() {
        Guard g(*this, "identifier_list");
        return match("ID") && nt_identifier_list_p();
    }

    bool nt_identifier_list_p() {
        Guard g(*this, "identifier_list_p");
        if (lookahead() == ",") return match(",") && match("ID") && nt_identifier_list_p();
        return true;
    }

    shared_ptr<ASTNode> nt_declarations() {
        Guard g(*this, "declarations");
        auto declsBlock = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, "Declarations");
        if (lookahead() == "VAR") {
            if (!match("VAR")) return nullptr;
            vector<Token> names;
            if (!nt_identifier_list(names) || !match(":")) return nullptr;
            DeclSpec spec = parseTypeSpec();
            if (!match(";")) return nullptr;
            insertNames(names, spec.kind, spec.type, spec.arrayNode);
            string declStr = makeVarDeclString(names, spec);
            auto varNode = make_shared<ASTNode>(ASTNodeKind::VAR_DECL, declStr);
            if (spec.arrayNode) {
                varNode->children.push_back(spec.arrayNode);
            }
            declsBlock->children.push_back(varNode);
            
            auto rest = nt_declarations_p();
            if (rest) {
                for (auto& child : rest->children) declsBlock->children.push_back(child);
            }
            return declsBlock;
        }
        return declsBlock;
    }

    shared_ptr<ASTNode> nt_declarations_p() {
        Guard g(*this, "declarations_p");
        auto declsBlock = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, "Declarations");
        if (lookahead() == "ID") {
            vector<Token> names;
            if (!nt_identifier_list(names) || !match(":")) return nullptr;
            DeclSpec spec = parseTypeSpec();
            if (!match(";")) return nullptr;
            insertNames(names, spec.kind, spec.type, spec.arrayNode);
            string declStr = makeVarDeclString(names, spec);
            auto varNode = make_shared<ASTNode>(ASTNodeKind::VAR_DECL, declStr);
            if (spec.arrayNode) varNode->children.push_back(spec.arrayNode);
            declsBlock->children.push_back(varNode);
            
            auto rest = nt_declarations_p();
            if (rest) {
                for (auto& child : rest->children) declsBlock->children.push_back(child);
            }
            return declsBlock;
        }
        return declsBlock;
    }

    shared_ptr<ASTNode> nt_subprogram_declarations() {
        Guard g(*this, "subprogram_declarations");
        auto subprogsBlock = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, "Subprograms");
        if (lookahead() == "FUNCTION" || lookahead() == "PROCEDURE") {
            auto subprog = nt_subprogram_declaration();
            if (!subprog) return nullptr;
            if (!match(";")) return nullptr;
            subprogsBlock->children.push_back(subprog);
            
            auto rest = nt_subprogram_declarations();
            if (rest) {
                for (auto& child : rest->children) subprogsBlock->children.push_back(child);
            }
            return subprogsBlock;
        }
        return subprogsBlock;
    }

    shared_ptr<ASTNode> nt_subprogram_declaration() {
        Guard g(*this, "subprogram_declaration");
        string subprogName;
        if (lookahead() == "FUNCTION") {
            if (!match("FUNCTION")) return nullptr;
            Token name = current();
            if (!match("ID")) return nullptr;
            subprogName = name.lexeme;
            insertNames({name}, SymbolKind::FUNCTION, SymbolType::VOID_TYPE);
            if (out != nullptr) symbols.beginScope(*out);
            if (!nt_arguments() || !match(":") || !nt_standard_type() || !match(";")) return nullptr;
        } else if (lookahead() == "PROCEDURE") {
            if (!match("PROCEDURE")) return nullptr;
            Token name = current();
            if (!match("ID")) return nullptr;
            subprogName = name.lexeme;
            insertNames({name}, SymbolKind::PROCEDURE, SymbolType::VOID_TYPE);
            if (out != nullptr) symbols.beginScope(*out);
            if (!nt_arguments() || !match(";")) return nullptr;
        } else {
            reportRuleError("subprogram_declaration");
            return nullptr;
        }
        
        auto declsNode = nt_declarations();
        auto bodyNode = nt_compound_statement();
        if (!bodyNode) return nullptr;
        
        if (out != nullptr) symbols.endScope(*out);
        
        auto subprogNode = make_shared<ASTNode>(ASTNodeKind::SUBPROGRAM_DECL, subprogName);
        if (declsNode) subprogNode->children.push_back(declsNode);
        if (bodyNode) subprogNode->children.push_back(bodyNode);
        return subprogNode;
    }

    bool nt_arguments() {
        Guard g(*this, "arguments");
        if (lookahead() == "(") return match("(") && nt_parameter_list() && match(")");
        return true;
    }

    bool nt_parameter_list() {
        Guard g(*this, "parameter_list");
        vector<Token> names;
        if (!nt_identifier_list(names) || !match(":")) return false;
        DeclSpec spec = parseTypeSpec();
        insertNames(names, SymbolKind::PARAMETER, spec.type, spec.arrayNode);
        
        while (lookahead() == ";") {
            if (!match(";")) return false;
            names.clear();
            if (!nt_identifier_list(names) || !match(":")) return false;
            spec = parseTypeSpec();
            insertNames(names, SymbolKind::PARAMETER, spec.type, spec.arrayNode);
        }
        return true;
    }

    bool nt_type() {
        Guard g(*this, "type");
        if (lookahead() == "integer" || lookahead() == "real") return nt_standard_type();
        if (lookahead() == "ARRAY") {
            return match("ARRAY") && match("[") && match("num") && match("..") &&
                   match("num") && match("]") && match("OF") && nt_standard_type();
        }
        reportRuleError("type");
        return false;
    }

    bool nt_standard_type() {
        Guard g(*this, "standard_type");
        if (lookahead() == "integer") return match("integer");
        if (lookahead() == "real") return match("real");
        reportRuleError("standard_type");
        return false;
    }

    shared_ptr<ASTNode> nt_compound_statement() {
        Guard g(*this, "compound_statement");
        if (out != nullptr) symbols.beginScope(*out);
        if (!match("begin")) return nullptr;
        
        auto blockDecls = nt_block_declarations();
        vector<shared_ptr<ASTNode>> stmts;
        if (!nt_optional_statements(stmts)) return nullptr;
        
        if (!match("end")) return nullptr;
        if (out != nullptr) symbols.endScope(*out);
        
        auto blockNode = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, "Block");
        if (blockDecls) {
            for (auto& child : blockDecls->children) blockNode->children.push_back(child);
        }
        for (auto& stmt : stmts) blockNode->children.push_back(stmt);
        return blockNode;
    }

    shared_ptr<ASTNode> nt_block_declarations() {
        Guard g(*this, "block_declarations");
        auto declsBlock = make_shared<ASTNode>(ASTNodeKind::BLOCK_STMT, "BlockDeclarations");
        if (lookahead() == "VAR") {
            if (!match("VAR")) return nullptr;
            vector<Token> names;
            if (!nt_identifier_list(names) || !match(":")) return nullptr;
            DeclSpec spec = parseTypeSpec();
            if (!match(";")) return nullptr;
            insertNames(names, spec.kind, spec.type, spec.arrayNode);
            string declStr = makeVarDeclString(names, spec);
            auto varNode = make_shared<ASTNode>(ASTNodeKind::VAR_DECL, declStr);
            if (spec.arrayNode) varNode->children.push_back(spec.arrayNode);
            declsBlock->children.push_back(varNode);
            
            while (lookahead() == "VAR") {
                if (!match("VAR")) break;
                names.clear();
                if (!nt_identifier_list(names) || !match(":")) break;
                spec = parseTypeSpec();
                if (!match(";")) break;
                insertNames(names, spec.kind, spec.type, spec.arrayNode);
                declStr = makeVarDeclString(names, spec);
                varNode = make_shared<ASTNode>(ASTNodeKind::VAR_DECL, declStr);
                if (spec.arrayNode) varNode->children.push_back(spec.arrayNode);
                declsBlock->children.push_back(varNode);
            }
        }
        return declsBlock;
    }

    bool nt_optional_statements(vector<shared_ptr<ASTNode>>& list) {
        Guard g(*this, "optional_statements");
        string la = lookahead();
        if (la == "ID" || la == "begin" || la == "IF" || la == "WHILE") return nt_statement_list(list);
        return true;
    }

    bool nt_statement_list(vector<shared_ptr<ASTNode>>& list) {
        Guard g(*this, "statement_list");
        auto stmt = nt_statement();
        if (!stmt) return false;
        list.push_back(stmt);
        return nt_statement_list_p(list);
    }

    bool nt_statement_list_p(vector<shared_ptr<ASTNode>>& list) {
        Guard g(*this, "statement_list_p");
        if (lookahead() == ";") {
            if (!match(";")) return false;
            string la = lookahead();
            if (la == "ID" || la == "begin" || la == "IF" || la == "WHILE") {
                auto stmt = nt_statement();
                if (!stmt) return false;
                list.push_back(stmt);
                return nt_statement_list_p(list);
            }
            return true;
        }
        return true;
    }

    shared_ptr<ASTNode> nt_statement() {
        Guard g(*this, "statement");
        string la = lookahead();
        if (la == "ID") {
            Token name = current();
            if (!match("ID")) return nullptr;
            if (out != nullptr && errors != nullptr) symbols.lookup(name, *errors, *out);
            auto idNode = make_shared<ASTNode>(ASTNodeKind::ID_NODE, name.lexeme);
            return nt_statement_tail(idNode);
        }
        if (la == "begin") return nt_compound_statement();
        if (la == "IF") {
            if (!match("IF")) return nullptr;
            auto cond = nt_expression();
            if (!match("THEN")) return nullptr;
            auto thenStmt = nt_statement();
            if (!match("ELSE")) return nullptr;
            auto elseStmt = nt_statement();
            auto ifNode = make_shared<ASTNode>(ASTNodeKind::IF_STMT, "IF");
            if (cond) ifNode->children.push_back(cond);
            if (thenStmt) ifNode->children.push_back(thenStmt);
            if (elseStmt) ifNode->children.push_back(elseStmt);
            return ifNode;
        }
        if (la == "WHILE") {
            match("WHILE");
            auto condNode = nt_expression();
            match("DO");
            auto bodyNode = nt_statement();
            auto whileNode = make_shared<ASTNode>(ASTNodeKind::WHILE_STMT, "WHILE");
            if (condNode) whileNode->children.push_back(condNode);
            if (bodyNode) whileNode->children.push_back(bodyNode);
            return whileNode;
        }
        reportRuleError("statement");
        return nullptr;
    }

    shared_ptr<ASTNode> nt_statement_tail(shared_ptr<ASTNode> inherited_id) {
        Guard g(*this, "statement_tail");
        string la = lookahead();
        if (la == "[" || la == ":=") {
            if (!nt_variable_prime()) return nullptr;
            if (!match(":=")) return nullptr;
            auto exprNode = nt_expression();
            auto assignNode = make_shared<ASTNode>(ASTNodeKind::ASSIGN_STMT, ":=");
            assignNode->children.push_back(inherited_id);
            if (exprNode) assignNode->children.push_back(exprNode);
            return assignNode;
        }
        if (la == "(" || la == ";" || la == "ELSE" || la == "end" || la == "$") {
            if (!nt_procedure_statement_prime()) return nullptr;
            return inherited_id;
        }
        reportRuleError("statement_tail");
        return nullptr;
    }

    bool nt_variable_prime() {
        Guard g(*this, "variable_prime");
        if (lookahead() == "[") return match("[") && nt_expression() && match("]");
        return true;
    }

    bool nt_procedure_statement_prime() {
        Guard g(*this, "procedure_statement_prime");
        if (lookahead() == "(") return match("(") && nt_expression_list() && match(")");
        return true;
    }

    bool nt_expression_list() {
        Guard g(*this, "expression_list");
        auto expr = nt_expression();
        if (!expr) return false;
        return nt_expression_list_p();
    }

    bool nt_expression_list_p() {
        Guard g(*this, "expression_list_p");
        if (lookahead() == ",") {
            if (!match(",")) return false;
            auto expr = nt_expression();
            if (!expr) return false;
            return nt_expression_list_p();
        }
        return true;
    }

    shared_ptr<ASTNode> nt_expression() {
        Guard g(*this, "expression");
        auto left = nt_simple_expression();
        if (!left) return nullptr;
        return nt_expression_prime(left);
    }

    shared_ptr<ASTNode> nt_expression_prime(shared_ptr<ASTNode> inherited_left) {
        Guard g(*this, "expression_prime");
        if (lookahead() == "relop") {
            string op = current().lexeme;
            match("relop");
            auto right = nt_simple_expression();
            auto opNode = make_shared<ASTNode>(ASTNodeKind::BINARY_EXPR, op);
            opNode->children.push_back(inherited_left);
            if (right) opNode->children.push_back(right);
            return opNode;
        }
        return inherited_left;
    }

    shared_ptr<ASTNode> nt_simple_expression() {
        Guard g(*this, "simple_expression");
        string la = lookahead();
        if (la == "addop") {
            if (!nt_sign()) return nullptr;
        }
        auto left = nt_term();
        if (!left) return nullptr;
        return nt_simple_expression_p(left);
    }

    shared_ptr<ASTNode> nt_simple_expression_p(shared_ptr<ASTNode> inherited_left) {
        Guard g(*this, "simple_expression_p");
        if (lookahead() == "addop") {
            string op = current().lexeme;
            match("addop");
            auto right = nt_term();
            auto opNode = make_shared<ASTNode>(ASTNodeKind::BINARY_EXPR, op);
            opNode->children.push_back(inherited_left);
            if (right) opNode->children.push_back(right);
            return nt_simple_expression_p(opNode);
        }
        return inherited_left;
    }

    shared_ptr<ASTNode> nt_term() {
        Guard g(*this, "term");
        auto left = nt_factor();
        if (!left) return nullptr;
        return nt_term_p(left);
    }

    shared_ptr<ASTNode> nt_term_p(shared_ptr<ASTNode> inherited_left) {
        Guard g(*this, "term_p");
        if (lookahead() == "mulop") {
            string op = current().lexeme;
            match("mulop");
            auto right = nt_factor();
            auto opNode = make_shared<ASTNode>(ASTNodeKind::BINARY_EXPR, op);
            opNode->children.push_back(inherited_left);
            if (right) opNode->children.push_back(right);
            return nt_term_p(opNode);
        }
        return inherited_left;
    }

    shared_ptr<ASTNode> nt_factor() {
        Guard g(*this, "factor");
        string la = lookahead();
        if (la == "ID") {
            Token name = current();
            match("ID");
            if (out != nullptr && errors != nullptr) symbols.lookup(name, *errors, *out);
            auto idNode = make_shared<ASTNode>(ASTNodeKind::ID_NODE, name.lexeme);
            if (!nt_factor_id_tail()) return nullptr;
            return idNode;
        }
        if (la == "num") {
            Token num = current();
            match("num");
            return make_shared<ASTNode>(ASTNodeKind::NUM_NODE, num.lexeme);
        }
        if (la == "(") {
            match("(");
            auto expr = nt_expression();
            match(")");
            return expr;
        }
        if (la == "not") {
            match("not");
            auto factor = nt_factor();
            auto notNode = make_shared<ASTNode>(ASTNodeKind::BINARY_EXPR, "not");
            if (factor) notNode->children.push_back(factor);
            return notNode;
        }
        reportRuleError("factor");
        return nullptr;
    }

    bool nt_factor_id_tail() {
        Guard g(*this, "factor_id_tail");
        if (lookahead() == "(") return match("(") && nt_expression_list() && match(")");
        return true;
    }

    bool nt_sign() {
        Guard g(*this, "sign");
        if (lookahead() == "addop") return match("addop");
        reportRuleError("sign");
        return false;
    }

public:
    bool parse(Lexer& lexObj, ErrorHandler& handler, ostream& traceOut) {
        lexer = &lexObj;
        currentToken = lexer->getNextToken();
        errors = &handler;
        out = &traceOut;
        indent = 0;
        resetSymbols();

        bool ok = nt_program();
        if (ok && lookahead() == ".") match(".");
        if (ok && lookahead() != ENDMARKER) {
            Token got = current();
            handler.add("Syntax", got.line, got.column,
                        "input not fully consumed, stopped at '" + got.lexeme + "'");
            ok = false;
        }
        while (symbols.current() != nullptr) {
            if (out != nullptr) symbols.endScope(*out);
            else break;
        }
        traceOut << (ok ? "RDP RESULT: ACCEPTED\n" : "RDP RESULT: REJECTED\n");
        return ok;
    }
};

#endif
