#include "common.h"
#include "lexer.h"
#include "lr_parser.h"
#include "predictive_parser.h"
#include "recursive_descent_parser.h"
#include "symbol_table.h"

using namespace std;

void printHeader(const string& title, ostream& out) {
    out << "\n========================================\n";
    out << title << "\n";
    out << "========================================\n";
}

void runFile(const string& filename, bool includeLr) {
    cout << "\n\n##################################################\n";
    cout << "SOURCE FILE: " << filename << "\n";
    cout << "##################################################\n";

    ErrorHandler lexerErrors;
    Lexer lexer;
    lexer.setTraceStream(cout);
    vector<Token> tokens = lexer.tokenizeFile(filename, lexerErrors);

    printHeader("LEXICAL ANALYZER", cout);
    printTokenStream(tokens, cout);
    lexerErrors.print(cout);

    printHeader("SEMANTIC ANALYZER", cout);
    ErrorHandler semanticErrors;
    SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(tokens, semanticErrors, cout);
    semanticErrors.print(cout);

    printHeader("RECURSIVE DESCENT PARSER", cout);
    ErrorHandler rdpErrors;
    Lexer rdpLexer;
    rdpLexer.init(filename, rdpErrors);
    RecursiveDescentParser rdp;
    rdp.parse(rdpLexer, rdpErrors, cout);
    rdpErrors.print(cout);

    auto astRoot = rdp.getASTRoot();
    if (astRoot) {
        cout << "\n=== ABSTRACT SYNTAX TREE DUMP ===\n";
        astRoot->print(cout, 0);
        cout << "=================================\n";
    }

    printHeader("NON-RECURSIVE PREDICTIVE PARSER", cout);
    ErrorHandler predictiveErrors;
    Lexer predLexer;
    predLexer.init(filename, predictiveErrors);
    PredictiveParser predictive;
    predictive.parse(predLexer, predictiveErrors, cout);
    predictiveErrors.print(cout);

    if (includeLr) {
        printHeader("LR PARSER", cout);
        ErrorHandler lrErrors;
        Lexer lrLexer;
        lrLexer.init(filename, lrErrors);
        LRParser lr;
        lr.parse(lrLexer, lrErrors, cout);
        lrErrors.print(cout);
    }
}

int main(int argc, char* argv[]) {
    bool includeLr = false;
    vector<string> files;
    bool dumpDocs = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--with-lr") includeLr = true;
        else if (arg == "--dump-docs") dumpDocs = true;
        else files.push_back(arg);
    }

    if (files.empty()) {
        files = {
            "test/valid_nested.pascal",
            "test/duplicate_same_scope.pascal",
            "test/undeclared_variable.pascal",
            "test/shadowing_valid.pascal",
            "test/mixed_errors.pascal"
        };
    }

    for (const string& file : files) runFile(file, includeLr);

    if (dumpDocs) {
        ofstream ofs;
        PredictiveParser pred;
        pred.prepare();
        LRParser lr;

        system("mkdir -p submission/docs");
        ofs.open("submission/docs/first_follow.txt");
        pred.printFirstFollow(ofs);
        ofs.close();

        ofs.open("submission/docs/ll1_table.txt");
        pred.printParsingTable(ofs);
        ofs.close();

        ofs.open("submission/docs/grammar_bnf.txt");
        pred.printGrammar(ofs);
        ofs.close();

        ofs.open("submission/docs/lr_table.txt");
        lr.dumpTables(ofs);
        ofs.close();
    }

    cout << "\n\nPROJECT COMBINATION STATUS\n";
    cout << "- Lexer: wired from earlier scanner work, now returns token/lexeme/line/column.\n";
    cout << "- Recursive descent parser: wired from week 9 grammar.\n";
    cout << "- Predictive parser: wired from week 10 LL(1) table approach with panic-mode recovery.\n";
    cout << "- Symbol table: integrated into parser actions for declarations and uses.\n";
    cout << "- LR parser: canonical LR(1) shift-reduce parser with action/goto tables and panic-mode recovery.\n";
    cout << "- Error handler: collects syntax and semantic diagnostics.\n";

    return 0;
}
