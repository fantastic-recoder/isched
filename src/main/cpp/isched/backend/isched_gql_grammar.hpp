/**
 * @file isched_gql_grammar.hpp
 * @brief PEGTL-based GraphQL grammar used by the isched backend.
 *
 * This header defines the lexical and syntactic grammar for a GraphQL subset/superset
 * as used by the isched server. It follows the structure and wording of the GraphQL
 * specification ("GraphQL.html" referenced in this project) and is thoroughly
 * verified via unit tests in `src/test/cpp/isched/isched_grammar_tests.cpp`.
 *
 * The grammar is implemented with tao::PEGTL primitives. Each grammar rule has a
 * Doxygen comment that maps it to the corresponding GraphQL spec rule where applicable,
 * explains constraints, and provides examples derived from the unit tests.
 *
 * High-level overview of groups:
 * - @ref gql_lexical Lexical elements (Whitespace, Comments, Name, Punctuators, Token)
 * - @ref gql_numbers Numeric value literals (IntValue, FloatValue)
 * - @ref gql_strings String value literals (quoted and block strings)
 * - @ref gql_util Utility separators and sequence helpers (TSeps, SeqWithComments)
 * - @ref gql_types Type system pieces mapped to existing demo rules (GqlType*, fields)
 * - @ref gql_exec Executable definitions (OperationType, SelectionSet)
 * - @ref gql_directives Directives and const values for schema/type-system
 * - @ref gql_schema SchemaDefinition and related non-terminals
 * - @ref gql_document Top-level Document rule and helpers
 *
 * See also: `generate_ast_and_log()` for debug tracing and AST emission used in tests.
 */

#ifndef ISCHED_ISCHED_GQL_GRAMMAR_HPP
#define ISCHED_ISCHED_GQL_GRAMMAR_HPP

#include <tuple>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <string>
#include <regex>
#include <algorithm>

namespace isched::v0_0_1::gql {
    namespace pegtl = tao::pegtl;
    using namespace pegtl;
    using pegtl::one;
    using pegtl::seq;
    using pegtl::star;
    using pegtl::ranges;
    using pegtl::sor;
    using pegtl::must_if;
    using pegtl::opt;
    using pegtl::at;
    using pegtl::plus;
    using parse_tree::node;

    using std::endl;
    using std::cout;
    using std::cerr;
    using std::unique_ptr;

    /** @defgroup gql_lexical GraphQL lexical elements
     *  @brief Low-level tokens: whitespace, comments, punctuators, names, and generic Token.
     *  @{ */

    /// GraphQL WhiteSpace — space or horizontal tab. Used by @ref Ignored and @ref TSeparator.
    /// @see GraphQL Spec: "WhiteSpace"
    struct Whitespace : one<' ', '\t'> {};

    /// GraphQL LineTerminator — LF or CR. Used by @ref Ignored and string handling.
    struct LineTerminator : one<'\n', '\r'> {};

    /// Internal convenience: any whitespace (space, tab, or line terminators).
    struct Ws : sor<LineTerminator,Whitespace> {};

    /** GraphQL Comment — starts with '#', runs to end-of-line or EOF.
     *  Example (positive): `# just a comment\n`, `# at EOF`
     */
    struct Comment : seq< one<'#'>, until<eolf>> {};

    /// GraphQL allows commas as Ignored between tokens (commas act as separators).
    struct Comma : one<','> {};

    /** Legacy/unused Punctuator set retained for backward-compatibility in older parts
     *  of the demo grammar. The canonical GraphQL punctuators used by @ref Token are
     *  provided by @ref TokenPunctuator. */
    struct Punctuator : one<'!', '$', '&', '(', ')', '*', '+', ',', '-', '.', '/', ';', '<', '=', '>', '?', '@', '[', ']', ':', '.'> {};

    /// ASCII Letter helper (A-Z, a-z) for Name.
    struct Letter : ranges<'A', 'Z', 'a', 'z'> {};

    /// ASCII Digit helper (0-9) for Name and numeric rules.
    struct Digit : ranges<'0', '9'> {};

    /// Non-zero digit helper for numeric grammars (1-9).
    struct NonZeroDigit : ranges<'1','9'> {};

    /** @defgroup gql_numbers Numeric value literals
     *  @brief GraphQL `IntValue` and `FloatValue` with spec-compliant constraints.
     *  @{ */

    /// IntValueCore := '0' | NonZeroDigit Digit*
    struct IntValueCore : sor<
        one<'0'>,
        seq< NonZeroDigit, star<Digit>>
    > {};

    /** IntValue := '-'? IntValueCore, not followed by '.', exponent, letter, or digit.
     *  - No leading zeros (except the literal `0`).
     *  - Not a prefix of a Float or Name token due to `not_at` lookahead.
     *  Examples (ok): `0`, `7`, `42`, `-1`
     *  Examples (reject): `01`, `+1`, `1.0`, `1e10`
     */
    struct IntValue : seq< opt< one<'-'> >, IntValueCore, not_at< sor< one<'.'>, one<'e','E'>, Letter, Digit > > > {};

    /// FractionalPart := '.' Digit+
    struct FractionalPart : seq< one<'.'>, plus<Digit> > {};
    /// ExponentPart := ('e'|'E') ('+'|'-')? Digit+
    struct ExponentPart   : seq< one<'e','E'>, opt< one<'+','-'> >, plus<Digit> > {};

    /// FloatValueCore := IntegerPart '.' Digits [Exponent]? | IntegerPart Exponent
    struct FloatValueCore : sor<
        // IntegerPart '.' Digits [Exponent]?
        seq< IntValueCore, FractionalPart, opt<ExponentPart> >,
        // IntegerPart Exponent
        seq< IntValueCore, ExponentPart >
    > {};

    /** FloatValue := '-'? FloatValueCore with `not_at` to avoid trailing alnum or '.'
     *  Examples (ok): `0.0`, `1.0`, `123.456`, `-0.123`, `1e10`, `10e+3`, `-3E5`, `10.0e-3`
     *  Examples (reject): `.5`, `1.`, `1e`, `+1.0`, `01.0`, `1.0a`, `1..0`
     */
    struct FloatValue : seq< opt< one<'-'> >, FloatValueCore, not_at< sor< Letter, Digit, one<'.'> > > > {};

    /** @} */

    /** @defgroup gql_strings GraphQL String value literals
     *  @brief Quoted strings and block strings with escapes per GraphQL spec.
     *  Quoted: '"' (StringCharacter | EscapeSequence)* '"'
     *  Block:  '"""' (BlockStringChar | EscapedTripleQuotes)* '"""'
     *  Newlines are allowed only in block strings. No indentation normalization is done here.
     *  @{ */
    struct HexDigit : sor< ranges<'0','9'>, ranges<'A','F'>, ranges<'a','f'> > {};
    struct UnicodeEscape : seq< one<'u'>, HexDigit, HexDigit, HexDigit, HexDigit > {};
    struct SimpleEscapeChar : one<'"','\\','/','b','f','n','r','t'> {};
    struct EscapeSequence : seq< one<'\\'>, sor< SimpleEscapeChar, UnicodeEscape > > {};
    struct StringCharacter : seq< not_at< sor< one<'"','\\'>, LineTerminator > >, pegtl::any > {};
    struct QuotedString : seq< one<'"'>, star< sor< StringCharacter, EscapeSequence > >, one<'"'> > {};

    // Block string support
    struct TripleQuote : pegtl::string<'"','"','"'> {};
    // Allow escaping of the triple quote sequence inside a block string
    struct EscapedTripleQuotes : seq< one<'\\'>, TripleQuote > {};
    // Any single char that does not start an unescaped triple quote
    struct BlockStringChar : seq< not_at< TripleQuote >, pegtl::any > {};
    struct BlockString : seq< TripleQuote, star< sor< EscapedTripleQuotes, BlockStringChar > >, TripleQuote > {};

    /** StringValue accepts block strings first to avoid backtracking into quoted strings
     *  when input begins with '"""'.
     *  Examples (ok): "" (empty), "hello", "\"quote\"\\\\", """multi\nline"""
     *  Examples (reject): unclosed quotes, raw newline in quoted, bad escapes like \x, bad \uXXXX
     */
    struct StringValue : sor< BlockString, seq< not_at< TripleQuote >, QuotedString > > {};

    /// NameStart: letter or underscore
    struct NameStart : sor<Letter,one<'_'>>{};

    /// NameContinues: letter, digit, or underscore
    struct NameContinues : sor<Letter,Digit,one<'_'>>{};

    /** GraphQL Name := [_A-Za-z] [_A-Za-z0-9]*
     *  Examples (ok): a, A, _, a1, _9, some_name, CamelCase, __typename, foo_bar_123
     *  Examples (reject): 1a, -a, a-b, a b, a$, a.b, ' (quote)
     */
    struct Name: seq<  NameStart, star<NameContinues>  >{};

    // Canonical GraphQL punctuators used by Token
    struct Ellipsis : pegtl::string<'.','.','.'> {};
    struct SinglePunctuator : one<'!','$','(',')',':','=', '@','[',']','{','}','|'> {};
    struct TokenPunctuator : sor< Ellipsis, SinglePunctuator > {};

    /** Token is a single lexical token per spec: StringValue | FloatValue | IntValue | Name | Punctuator.
     *  Ordering ensures longest/more specific matches first to avoid ambiguity.
     */
    struct Token : sor< StringValue, FloatValue, IntValue, Name, TokenPunctuator > {};

    /** Ignored tokens per GraphQL spec — skipped between significant tokens.
     *  Includes: UnicodeBOM, WhiteSpace, LineTerminator, Comma, Comment.
     *  Comments may end at newline or EOF. */
    struct UnicodeBOM : pegtl::utf8::bom {};
    struct Ignored : sor< UnicodeBOM, Whitespace, LineTerminator, Comma, Comment > {};
    /** @} */


    /** @defgroup gql_util Utility separators and helpers
     *  @brief Braces, separators, and template to weave separators between rules.
     *  @{ */
    /// Left brace '{' used by selection sets and schema blocks.
    struct Beg : one<'{'> { };

    /// Right brace '}' used by selection sets and schema blocks.
    struct End : one<'}'> { };

    /// A single separator unit: whitespace or comment.
    struct TSeparator : sor<Ws, Comment> { };

    /// Zero or more separators; used widely between grammar items.
    struct TSeps : star<TSeparator> { };

    /**
     * @tparam TSeparator A separator rule (e.g., @ref TSeps)
     * @tparam TRules     A sequence of rules to be interleaved with separators
     * @brief Utility template to generate `seq<rule0, sep, rule1, sep, ..., rulen, sep>`.
     * Used to keep grammar readable while allowing comments/whitespace between items.
     */
    template<typename TSeparator, typename... TRules>
    struct SeqWithComments;

    template<typename TSeparator, typename TRule0, typename... TRulesRest>
    struct SeqWithComments<TSeparator, TRule0, TRulesRest...>
            : seq<TRule0, TSeparator, SeqWithComments<TSeparator, TRulesRest...> > {
    };

    template<typename TSeparator, typename TRule0>
    struct SeqWithComments<TSeparator, TRule0>
            : seq<TRule0, TSeparator> {
    };
    /** @} */

    /** @defgroup gql_exec Executable definitions
     *  @brief Minimal executable constructs used in tests (SelectionSet proxy, OperationType).
     *  @{ */

    /** GqlSubQuery ~ minimal SelectionSet used in tests:
     *  `{` Name? GqlSubQuery? `}` allowing nested selections. */
    struct GqlSubQuery : SeqWithComments<
                TSeps,
                Beg,
                opt<Name>,
                opt<GqlSubQuery>,
                End
            > {
    };

    /// Optional "query" keyword followed by a @ref GqlSubQuery selection set.
    struct GqlQuery : SeqWithComments<
                TSeps,
                opt<string<'q', 'u', 'e', 'r', 'y'> >,
                GqlSubQuery
            > {
    };

    /** @defgroup gql_types Type grammar building blocks (demo)
     *  @brief Basic type system constructs for tests and examples.
     *  @{ */

    /// NamedType (identifier) in our demo grammar.
    struct GqlTypeName : SeqWithComments<
                TSeps,
                pegtl::identifier,
                TSeps
    >{};

    /// Built-in scalar: String
    struct GqlStringType : SeqWithComments<
                TSeps,
                pegtl::string<'S','t','r','i','n','g'>,
                TSeps
    >{};

    /// Built-in scalar: Int
    struct GqlTypeInt : SeqWithComments<
                TSeps,
                pegtl::string<'I','n','t'>,
                TSeps
    >{};

    /// Built-in scalar: Float
    struct GqlTypeFloat : SeqWithComments<
                TSeps,
                pegtl::string<'F','l','o','a','t'>,
                TSeps
    >{};

    /// Built-in scalar: Boolean
    struct GqlTypeBoolean : SeqWithComments<
                TSeps,
                pegtl::string<'B','o','o','l','e','a','n'>,
                TSeps
    >{};

    /// Built-in scalar: ID
    struct GqlTypeID : SeqWithComments<
                TSeps,
                pegtl::string<'I','D'>,
                TSeps
    >{};

    /// Optional non-null marker '!'
    struct GqlNonNullType : opt<one<'!'>>{};

    struct GqlBuiltInType;
    struct GqlType;

    /// List type: '[' Type ']'
    struct GqlArray : SeqWithComments<
                TSeps,
                one<'['>,
                GqlType,
                one<']'>
    >{};

    /// Reference to a named type (identifier).
    struct GqlTypeRef: pegtl::identifier{};

    /// Type := (Array | BuiltIn | TypeRef) NonNull?
    struct GqlType:seq<
              sor<
                    GqlArray,
                    GqlBuiltInType,
                    GqlTypeRef
              >,
              GqlNonNullType
    >{};

    /// Any built-in scalar or nested array
    struct GqlBuiltInType:sor<
                    GqlStringType,
                    GqlTypeInt,
                    GqlTypeFloat,
                    GqlTypeBoolean,
                    GqlTypeID,
                    GqlArray
                >{};

    /// Field definition: Name ':' Type
    struct GqlTypeField : SeqWithComments<
                TSeps,
                GqlTypeName,
                pegtl::string<':'>,
                GqlType
    >{};

    /// Field list: '{' Field+ '}'
    struct GqlTypeFields : SeqWithComments<
        TSeps,
        Beg,
        plus<GqlTypeField>,
        End
    >{};

    /// TypeDefinition (demo): 'type' Name '{' Field+ '}' with separators.
    struct GqlTypeDef : seq<
                TSeps,
                pegtl::string<'t', 'y', 'p', 'e'>,
                plus<TSeparator>,
                GqlTypeName,
                GqlTypeFields
            > {
    };
    /** @} */

    // ===== GraphQL Core Grammar: Document and Schema (per GraphQL.html) =====
    // Forward declarations for grammar non-terminals we reference
    struct Definition;
    struct ExecutableDefinition;
    struct OperationDefinition;
    struct FragmentDefinition;
    struct TypeSystemDefinitionOrExtension;
    struct TypeSystemDefinition;
    struct TypeSystemExtension;
    struct TypeDefinition; // map to GqlTypeDef
    struct DirectiveDefinition; // placeholder
    struct SchemaDefinition;
    struct RootOperationTypeDefinition;
    struct OperationType;
    struct VariablesDefinition; // placeholder for now
    struct Directives; // placeholder for now
    struct SelectionSet; // alias to our subquery structure
    struct Description; // placeholder (StringValue) – optional where used

    /// OperationType: 'query' | 'mutation' | 'subscription'
    struct OperationType : sor<
            string<'q','u','e','r','y'>,
            string<'m','u','t','a','t','i','o','n'>,
            string<'s','u','b','s','c','r','i','p','t','i','o','n'>
    >{};

    /// SelectionSet proxy used in tests (maps to @ref GqlSubQuery).
    struct SelectionSet : GqlSubQuery {};

    // Placeholders for future expansion (optional in places used)
    struct VariablesDefinition : seq<> {};

    /** @defgroup gql_directives GraphQL directives (const) and values
     *  @brief Directive syntax used in schema/type-system positions and const values.
     *  @{ */
    struct TrueKeyword  : pegtl::string<'t','r','u','e'> {};
    struct FalseKeyword : pegtl::string<'f','a','l','s','e'> {};
    struct NullKeyword  : pegtl::string<'n','u','l','l'> {};

    struct BooleanValueConst : sor< TrueKeyword, FalseKeyword > {};
    struct NullValueConst : NullKeyword {};

    // Allow enum-like values via Name in const positions; numbers/strings reuse terminals.
    struct ValueConst : sor< StringValue, FloatValue, IntValue, BooleanValueConst, NullValueConst, Name > {};

    /// ArgumentConst := Name ':' ValueConst
    struct ArgumentConst : SeqWithComments<
            TSeps,
            Name,
            one<':'>,
            ValueConst
    > {};

    /** ArgumentsConst := '(' [Ignored*] ArgumentConst ( [Ignored*] ArgumentConst )* [Ignored*] ')'
     *  Commas are permitted within Ignored per spec; TSeps tolerated between elements.
     */
    struct ArgumentsConst : seq<
            one<'('>,
            star<Ignored>,
            ArgumentConst,
            star< star<Ignored>, ArgumentConst >,
            star<Ignored>,
            one<')'>
    > {};

    /// DirectiveConst := '@' Name ArgumentsConst?
    struct DirectiveConst : seq<
            one<'@'>,
            Name,
            opt< ArgumentsConst >
    > {};

    /// DirectivesConst := DirectiveConst (TSeps DirectiveConst)* (one or more)
    struct DirectivesConst : seq< DirectiveConst, star< TSeps, DirectiveConst > > {};
    /** @} */
    // Description: GraphQL allows an optional description preceding many definitions.
    struct Description : StringValue {};
    // Placeholder disabled to prevent empty matches that could cause non-consuming loops in Document
    struct FragmentDefinition : failure {};

    /** @defgroup gql_schema GraphQL schema/type-system and operation definitions
     *  @brief SchemaDefinition and related rules (plus OperationDefinition used in tests).
     *  @{ */
    // OperationDefinition: (Description? OperationType Name? VariablesDefinition? Directives? SelectionSet) | SelectionSet
    struct OperationDefinition : sor<
            SeqWithComments< TSeps,
                opt<Description>,
                OperationType,
                opt<Name>,
                opt<VariablesDefinition>,
                opt<DirectivesConst>,
                SelectionSet
            >,
            SelectionSet
    >{};

    /// ExecutableDefinition := OperationDefinition | FragmentDefinition (disabled)
    struct ExecutableDefinition : sor< OperationDefinition, FragmentDefinition > {};

    /// TypeSystemDefinitionOrExtension := TypeSystemDefinition | TypeSystemExtension (disabled)
    struct TypeSystemDefinitionOrExtension : sor< struct TypeSystemDefinition, struct TypeSystemExtension > {};

    /// TypeDefinition maps to our demo @ref GqlTypeDef
    struct TypeDefinition : GqlTypeDef {};

    // Placeholders: not implemented — set to failure to avoid empty matches in loops
    struct DirectiveDefinition : failure {};
    struct TypeSystemExtension : failure {};

    /** SchemaDefinition := Description? 'schema' DirectivesConst? '{' RootOperationTypeDefinition+ '}'
     *  Examples (ok):
     *  - `schema { query: Query }`
     *  - `"""desc""" schema @a(flag: true) { query: Q mutation: M }`
     */
    struct SchemaDefinition : seq<
            TSeps,
            opt<Description>,
            TSeps,
            string<'s','c','h','e','m','a'>,
            TSeps,
            opt<DirectivesConst>,
            TSeps,
            Beg,
            TSeps,
            plus< seq< RootOperationTypeDefinition, TSeps > >,
            End,
            TSeps
    > {};

    /// RootOperationTypeDefinition := OperationType ':' NamedType (uses @ref GqlTypeName)
    struct RootOperationTypeDefinition : SeqWithComments<
            TSeps,
            OperationType,
            one<':'>,
            GqlTypeName
    > {};

    /// TypeSystemDefinition := SchemaDefinition | TypeDefinition | DirectiveDefinition (disabled)
    struct TypeSystemDefinition : sor< SchemaDefinition, TypeDefinition, DirectiveDefinition > {};

    /// Definition := ExecutableDefinition | TypeSystemDefinitionOrExtension
    struct Definition : sor< ExecutableDefinition, TypeSystemDefinitionOrExtension > {};

    /// ExecutableDocument (not used directly in tests): ExecutableDefinition+
    struct ExecutableDocument : plus< ExecutableDefinition > {};

    /** @defgroup gql_document Top-level Document rule and helpers
     *  @brief `Document` requires full input consumption and permits Ignored between/around definitions.
     *  @{ */
    /// Helper: zero or more Ignored tokens
    struct IgnoredMany : star< Ignored > {};

    /** Document := Ignored* (Definition Ignored*)+ EOF
     *  Requires at least one Definition and consumption to EOF.
     *  Guarded against empty-matching definitions by setting placeholders to `failure`.
     */
    struct Document : seq<
            IgnoredMany,
            plus< seq< Definition, IgnoredMany > >,
            eof
    > {};
    /** @} */

    /**
     * @brief Parse-tree selector used by tests to choose which nodes store their content.
     * Extend this list if downstream tooling needs additional nodes in the AST output.
     */
    template<typename TRule>
    using GqlSelector = parse_tree::selector<
        TRule,
        parse_tree::store_content::on<
            GqlQuery, Name, GqlTypeDef, GqlTypeField,GqlTypeName,GqlType,GqlTypeRef,
            GqlStringType,GqlTypeInt,GqlTypeFloat,GqlTypeBoolean,GqlTypeID,GqlArray,GqlNonNullType,
            // New grammar nodes for Document/Schema
            Document, Definition, ExecutableDefinition, OperationDefinition, OperationType,
            SchemaDefinition, RootOperationTypeDefinition,
            // Description
            Description,
            // Directives/Values
            DirectiveConst, DirectivesConst, ArgumentsConst, ArgumentConst, ValueConst,
            TrueKeyword, FalseKeyword, NullKeyword,
            // Numeric terminals
            IntValue, FloatValue,
            // String terminal
            StringValue,
            // Token
            Token
        >
    >;

    //template<typename> inline constexpr const char *error_message = nullptr;
    //template<> inline constexpr auto error_message<Beg> = "expected {";
    //template<> inline constexpr auto error_message<End> = "incomplete query, expected }";
    //template<> inline constexpr auto error_message<name> = "GqlError parsing name";

    //struct GqlError {
    //    template<typename Rule>
    //    static constexpr auto message = error_message<Rule>;
    //};

    //template<typename Rule>
    //using control = must_if<GqlError>::control<Rule>;

    /// Separator used in test output for pretty-printing the AST with Graphviz DOT.
    constexpr static const char *const K_OUTPUT_SEP =
"************************************************************************";

    /**
     * @brief Parse the input with the given grammar, emit a Graphviz DOT tree to stdout, and optionally trace.
     * @tparam TGrammar Top-level PEGTL rule to parse.
     * @param pName Label used in logs.
     * @param in PEGTL string_input (will be restarted to print trace on failure or when requested).
     * @param p_trace_on_success If true, run a PEGTL trace after successful parse as well.
     * @return (ok, root) where ok indicates parse success, and root is the parse tree (non-null on success).
     */
    template<typename TGrammar>
    std::tuple<bool,std::unique_ptr<node>>
    generate_ast_and_log(const std::string &pName, pegtl::string_input<>& in, bool p_trace_on_success=false) {
        std::unique_ptr<node> myRoot = pegtl::parse_tree::parse<
            TGrammar, GqlSelector /*, ns_pegtl::nothing, control*/
        >(in);
        spdlog::debug("\n\nAST of \"{}\":\n{}", pName, K_OUTPUT_SEP);
        bool myParsingOk;
        if (myRoot) {
            pegtl::parse_tree::print_dot(std::cout, *myRoot);
            std::cout << endl
                    << K_OUTPUT_SEP << endl
                    << endl;
            myParsingOk = true;
            if (p_trace_on_success) {
                in.restart();
                tao::pegtl::complete_trace<TGrammar>(in);
            }
        } else {
            spdlog::error(R"("{}" error / no AST generated!? Input rest:"{}".)", pName,
                          std::string(in.begin(), in.size()));
            in.restart();
            tao::pegtl::complete_trace<TGrammar>(in);
            myParsingOk = false;
        }
        return {myParsingOk,std::move(myRoot)};
    }

}
#endif //ISCHED_ISCHED_GQL_GRAMMAR_HPP