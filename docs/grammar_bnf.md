# Pascal Subset Grammar Used

This is the transformed LL(1)-friendly grammar from the previous lab work.

```text
program -> PROGRAM ID ( identifier_list ) ; declarations subprogram_declarations compound_statement
identifier_list -> ID identifier_list_p
identifier_list_p -> , ID identifier_list_p | eps
declarations -> declarations_p
declarations_p -> VAR identifier_list : type ; declarations_p | eps
type -> standard_type | ARRAY [ num .. num ] OF standard_type
standard_type -> integer | real
subprogram_declarations -> subprogram_declarations_p
subprogram_declarations_p -> subprogram_declaration ; subprogram_declarations_p | eps
subprogram_declaration -> subprogram_head declarations compound_statement
subprogram_head -> FUNCTION ID arguments : standard_type ; | PROCEDURE ID arguments ;
arguments -> ( parameter_list ) | eps
parameter_list -> identifier_list : type parameter_list_p
parameter_list_p -> ; identifier_list : type parameter_list_p | eps
compound_statement -> begin optional_statements end
optional_statements -> statement_list | eps
statement_list -> statement statement_list_p
statement_list_p -> ; statement statement_list_p | eps
statement -> ID statement_tail | compound_statement | IF expression THEN statement ELSE statement | WHILE expression DO statement
statement_tail -> variable_prime := expression | procedure_statement_prime
variable_prime -> [ expression ] | eps
procedure_statement_prime -> ( expression_list ) | eps
expression_list -> expression expression_list_p
expression_list_p -> , expression expression_list_p | eps
expression -> simple_expression expression_prime
expression_prime -> relop simple_expression | eps
simple_expression -> term simple_expression_p | sign term simple_expression_p
simple_expression_p -> addop term simple_expression_p | eps
term -> factor term_p
term_p -> mulop factor term_p | eps
factor -> ID factor_id_tail | num | ( expression ) | not factor
factor_id_tail -> ( expression_list ) | eps
sign -> addop
```
