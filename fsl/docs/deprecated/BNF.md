# FSL BNF

This is the raw BNF converted from the bison grammar

```
<top> ::= <source_line> | <top> <source_line>

<source_line> ::= <transform_definition> | <prolog_definition> | <declaration>

<transform_definition> ::= TRANSFORM <id> '{' '}' | TRANSFORM <id> '{' <transform_statements> '}'

<transform_statements> ::= <transform_statement> | <transform_statements> <transform_statement>

<transform_statement> ::= PROLOG <id> | <isa_decl> | <uarch_decl> | <ioput_decl> | <variable_decl> | <selection_statement> | <constraints_definition> | <conversion_definition> | <setof_definition>

<setof_definition> ::= SETOF <id> '=' <chained_id_list> '.' <chained_method_list>

<chained_method_list> ::= <chained_method_list> '.' <known_method_decl> | <known_method_decl>

<known_method_decl> ::= <known_method> '(' <opt_arg> ')'

<opt_arg> ::= /* empty */ | <id> | <constant> | '{' '}' | '*' | '{' <concatenate_list> '}'

<prolog_definition> ::= PROLOG <id> '{' '}' | PROLOG <id> '{' <prolog_statements> '}'

<prolog_statements> ::= <prolog_statement> | <prolog_statements> <prolog_statement>

<prolog_statement> ::= ISA <id> | UARCH <id> | IOPUT <id>

<isa_decl> ::= ISA <id>

<uarch_decl> ::= UARCH <id>

<ioput_decl> ::= IOPUT <id>

<variable_definition> ::= <type_specifier> <id> '=' <assignment_expression> | <type_specifier> <id> '=' '{' <concatenate_list> '}'

<variable_decl> ::= <type_specifier> <arg_expr_list> | <variable_definition>

<constraints_definition> ::= CONSTRAINTS <opt_id> '{' <constraints_statements> '}' | CONSTRAINTS <id> '(' <arg_expr_list> ')' '{' <constraints_statements> '}'

<opt_id> ::= /* empty */ | <id>

<constraints_statements> ::= <constraints_statement> | <constraints_statements> <constraints_statement>

<constraints_statement> ::= <pass_fail_statement> | <chained_id_list> <comparison_operator> <chained_id_list> | <chained_id_list> <comparison_operator> <constant> | <chained_id_list> <comparison_operator> <chained_id_list> LEFT_OP <constant> | <chained_id_list> '.' <known_method_decl> <comparison_operator> <chained_id_list> '.' <known_method_decl> | <chained_id_list> '.' <known_method_decl> <comparison_operator> <constant> | <selection_statement>

<comparison_operator> ::= LE_OP | GE_OP | EQ_OP | NE_OP

<conversion_definition> ::= CONVERSION <opt_id> '{' <conversion_statements> '}' | CONVERSION <id> '(' <arg_expr_list> ')' '{' <conversion_statements> '}'

<conversion_statements> ::= <conversion_statement> | <conversion_statements> <conversion_statement>

<conversion_statement> ::= <pass_fail_statement> | <variable_decl> | <encoding_decl> | <encoding_definition> | <instr_decl> | <instr_definition> | <chained_id_list> '.' <known_method_decl> | <chained_id_list> '.' REPLACE '(' <comma_sep_list> ')'

<concatenate_list> ::= <concatenate_elem> | <concatenate_list> ',' <concatenate_elem>

<concatenate_elem> ::= <id> | <id> <range_list> <opt_dot_id> | OPC | <constant> | <known_method> '=' <constant>

<opt_dot_id> ::= /* empty */ | '.' <id>

<comma_sep_list> ::= <id> | <comma_sep_list> ',' <id>

<range_list> ::= '[' <constant> ']' | '[' <constant> ':' <constant> ']' | <range_list> '[' <constant> ']' | <range_list> '[' <constant> ':' <constant> ']'

<chained_id_list> ::= <id> | <chained_id_list> '.' <id>

<known_method> ::= MNEMONIC | ENCODE_ORDER | WRITEPORTS | READPORTS | REQUIREDBITS | ENCODING | OPC | SRC | DST | RSX | IMM | TYPE | HASATTR | MORPH

<instr_decl> ::= INSTR <id>

<instr_definition> ::= INSTR <id> '(' <arg_assignment_list> ')' | INSTR <id> '(' '{' <concatenate_list> '}' ')' | INSTR <id> '(' '{' <encode_list> '}' ')' | INSTR <id> '(' <chained_id_list> '.' <known_method> '(' <id> ')' ')' | INSTR <id> '(' <known_method> '(' <id> ')' ')'

<encode_list> ::= <encode_elem> | <encode_list> ',' <encode_elem>

<encode_elem> ::= <id> '[' <constant> ']' '.' ENCODING

<encoding_decl> ::= ENCODING <id>

<encoding_definition> ::= ENCODING <id> '(' <arg_assignment_list> ')' | ENCODING <id> '(' '{' <concatenate_list> '}' ')'

<arg_assignment_list> ::= <arg_assignment> | <arg_assignment_list> ',' <arg_assignment>

<arg_assignment> ::= <known_method> '=' <constant> | <known_method> '=' '{' <arg_expr_list> '}' | <id> '=' '{' <arg_expr_list> '}'

<pass_fail_statement> ::= PASS | FAIL

<primary_expression> ::= <id> | <constant> | STRING_LITERAL | '(' <expression> ')'

<postfix_expression> ::= <primary_expression> | <postfix_expression> '[' <expression> ']' | <postfix_expression> '(' <opt_arg_expr_list> ')' | <postfix_expression> '.' <id> | <postfix_expression> INC_OP | <postfix_expression> DEC_OP | <postfix_expression> '.' ENCODING '(' ')'

<opt_arg_expr_list> ::= /* empty */ | <arg_expr_list>

<arg_expr_list> ::= <assignment_expression> | <arg_expr_list> ',' <assignment_expression>

<unary_expression> ::= <postfix_expression> | INC_OP <unary_expression> | DEC_OP <unary_expression> | <unary_operator> <cast_expression>

<unary_operator> ::= '&' | '*' | '+' | '-' | '~' | '!'

<cast_expression> ::= <unary_expression> | '(' <type_name> ')' <cast_expression>

<multiplicative_expression> ::= <cast_expression> | <multiplicative_expression> '*' <cast_expression> | <multiplicative_expression> '/' <cast_expression> | <multiplicative_expression> '%' <cast_expression>

<additive_expression> ::= <multiplicative_expression> | <additive_expression> '+' <multiplicative_expression> | <additive_expression> '-' <multiplicative_expression>

<shift_expression> ::= <additive_expression> | <shift_expression> LEFT_OP <additive_expression> | <shift_expression> RIGHT_OP <additive_expression>

<relational_expression> ::= <shift_expression> | <relational_expression> '<' <shift_expression> | <relational_expression> '>' <shift_expression> | <relational_expression> LE_OP <shift_expression> | <relational_expression> GE_OP <shift_expression>

<equality_expression> ::= <relational_expression> | <equality_expression> EQ_OP <relational_expression> | <equality_expression> NE_OP <relational_expression>

<and_expression> ::= <equality_expression> | <and_expression> '&' <equality_expression>

<exclusive_or_expression> ::= <and_expression> | <exclusive_or_expression> '^' <and_expression>

<inclusive_or_expression> ::= <exclusive_or_expression> | <inclusive_or_expression> '|' <exclusive_or_expression>

<logical_and_expression> ::= <inclusive_or_expression> | <logical_and_expression> AND_OP <inclusive_or_expression>

<logical_or_expression> ::= <logical_and_expression> | <logical_or_expression> OR_OP <logical_and_expression>

<conditional_expression> ::= <logical_or_expression> | <logical_or_expression> '?' <expression> ':' <conditional_expression>

<assignment_expression> ::= <conditional_expression> | <unary_expression> <assignment_operator> <assignment_expression>

<expression> ::= <assignment_expression> | <expression> ',' <assignment_expression>

<assignment_operator> ::= '='

<declaration> ::= <declaration_specifiers> ';' | <declaration_specifiers> <init_declarator_list> ';'

<declaration_specifiers> ::= <storage_class_specifier> | <storage_class_specifier> <declaration_specifiers> | <type_specifier> | <type_specifier> <declaration_specifiers>

<init_declarator_list> ::= <init_declarator> | <init_declarator_list> ',' <init_declarator>

<init_declarator> ::= <declarator> | <declarator> '=' <initializer>

<storage_class_specifier> ::= EXTERN | AUTO

<type_specifier> ::= GPR | CSR | UN_CONST | S_CONST | STRING

<declarator> ::= <direct_declarator>

<direct_declarator> ::= <id> | '(' <declarator> ')' | <direct_declarator> '[' '*' ']' | <direct_declarator> '[' <constant> ']' | <direct_declarator> '[' <constant> ':' <constant> ']' | <direct_declarator> '[' ']' | <direct_declarator> '(' <parameter_list> ')' | <direct_declarator> '(' <identifier_list> ')' | <direct_declarator> '(' ')'

<parameter_list> ::= <parameter_declaration> | <parameter_list> ',' <parameter_declaration>

<parameter_declaration> ::= <declaration_specifiers> <declarator> | <declaration_specifiers>

<identifier_list> ::= <id> | <identifier_list> ',' <id>

<type_name> ::= <specifier_qualifier_list>

<specifier_qualifier_list> ::= <type_specifier> | <type_specifier> <specifier_qualifier_list>

<initializer> ::= <assignment_expression> | '{' <initializer_list> '}' | '{' <initializer_list> ',' '}'

<initializer_list> ::= <initializer> | <initializer_list> ',' <initializer>

<statement> ::= <pass_fail_statement> | <compound_statement> | <expression_statement> | <selection_statement> | <iteration_statement>

<compound_statement> ::= '{' '}' | '{' <block_item_list> '}'

<block_item_list> ::= <block_item> | <block_item_list> <block_item>

<block_item> ::= <declaration> | <statement>

<expression_statement> ::= ';' | <expression> ';'

<selection_statement> ::= IF '(' <expression> ')' <statement> %prec NO_ELSE | IF '(' <expression> ')' <statement> ELSE <statement>

<iteration_statement> ::= FOR '(' <expression_statement> <expression_statement> ')' <statement> | FOR '(' <expression_statement> <expression_statement> <expression> ')' <statement> | FOR '(' <declaration> <expression_statement> ')' <statement> | FOR '(' <declaration> <expression_statement> <expression> ')' <statement>

<id> ::= ID

<constant> ::= CONSTANT | HEX_CONST | VLOG_CONST | QSTRING
```
