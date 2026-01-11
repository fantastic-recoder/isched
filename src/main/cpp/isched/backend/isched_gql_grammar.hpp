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
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <string>
#include <expected>
#include <tao/pegtl/ascii.hpp>
#include <tao/pegtl/must_if.hpp>
#include <tao/pegtl/rules.hpp>
#include <tao/pegtl/string_input.hpp>
#include <tao/pegtl/utf8.hpp>
#include <utility>
#include <vector>

#include "isched_gql_error.hpp"

namespace isched::v0_0_1::gql {
    struct Value;
    struct DefaultValue;
    struct NonNullType;
    struct ListType;
    struct DirectivesConst;
    struct Description;
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

    /// @see GraphQL Spec: "WhiteSpace"
    struct Whitespace : one<' ', '\t'> {};

    /// @see GraphQL Spec: "LineTerminator"
    struct LineTerminator : one<'\n', '\r'> {};

    /// @see GraphQL Spec: "Ignored"
    struct WhitespaceOnly : sor<LineTerminator,Whitespace> {};

    /// @see GraphQL Spec: "Comment"
    struct Comment : seq< one<'#'>, until<eolf>> {};

    /// @see GraphQL Spec: "Comma"
    struct Comma : one<','> {};

    /// @see GraphQL Spec: "Punctuator"
    struct Punctuator : one<'!', '$', '&', '(', ')', '*', '+', ',', '-', '.', '/', ';', '<', '=', '>', '?', '@', '[', ']', ':', '.'> {};

    /// @see GraphQL Spec: "Letter"
    struct Letter : ranges<'A', 'Z', 'a', 'z'> {};

    /// @see GraphQL Spec: "Digit"
    struct Digit : ranges<'0', '9'> {};

    /// @see GraphQL Spec: "NonZeroDigit"
    struct NonZeroDigit : ranges<'1','9'> {};

    /** @defgroup gql_numbers Numeric value literals
     *  @brief GraphQL `IntValue` and `FloatValue` with spec-compliant constraints.
     *  @{ */

    /// @see GraphQL Spec: "IntegerPart"
    struct IntegerPart : seq< opt< one<'-'> >, sor<
        one<'0'>,
        seq< NonZeroDigit, star<Digit>>
    > > {};

    /// @see GraphQL Spec: "IntValue"
    struct IntValue : seq< IntegerPart, not_at< sor< one<'.'>, one<'e','E'>, Letter, Digit > > > {};

    /// @see GraphQL Spec: "FractionalPart"
    struct FractionalPart : seq< one<'.'>, plus<Digit> > {};

    /// @see GraphQL Spec: "ExponentPart"
    struct ExponentPart   : seq< one<'e','E'>, opt< one<'+','-'> >, plus<Digit> > {};

    /// Internal helper for FloatValue
    struct FloatValueCore : sor<
        seq< IntegerPart, FractionalPart, opt<ExponentPart> >,
        seq< IntegerPart, ExponentPart >
    > {};

    /// @see GraphQL Spec: "FloatValue"
    struct FloatValue : seq< FloatValueCore, not_at< sor< Letter, Digit, one<'.'> > > > {};

    /** @} */

    /** @defgroup gql_strings GraphQL String value literals
     *  @brief Quoted strings and block strings with escapes per GraphQL spec.
     *  @{ */

    /// @see GraphQL Spec: "HexDigit"
    struct HexDigit : sor< ranges<'0','9'>, ranges<'A','F'>, ranges<'a','f'> > {};

    /// @see GraphQL Spec: "UnicodeEscape"
    struct UnicodeEscape : seq< one<'u'>, HexDigit, HexDigit, HexDigit, HexDigit > {};

    /// Internal helper for EscapeSequence
    struct SimpleEscapeChar : one<'"','\\','/','b','f','n','r','t'> {};

    /// @see GraphQL Spec: "EscapeSequence"
    struct EscapeSequence : seq< one<'\\'>, sor< SimpleEscapeChar, UnicodeEscape > > {};

    /// @see GraphQL Spec: "StringCharacter"
    struct StringCharacter : seq< not_at< sor< one<'"','\\'>, LineTerminator > >, pegtl::any > {};

    /// Internal helper for StringValue
    struct QuotedString : seq< one<'"'>, star< sor< StringCharacter, EscapeSequence > >, one<'"'> > {};

    /// @see GraphQL Spec: "TripleQuote"
    struct TripleQuote : pegtl::string<'"','"','"'> {};

    /// @see GraphQL Spec: "EscapedTripleQuotes"
    struct EscapedTripleQuotes : seq< one<'\\'>, TripleQuote > {};

    /// @see GraphQL Spec: "BlockStringCharacter"
    struct BlockStringChar : seq< not_at< TripleQuote >, pegtl::any > {};

    /// Internal helper for StringValue
    struct BlockString : seq< TripleQuote, star< sor< EscapedTripleQuotes, BlockStringChar > >, TripleQuote > {};

    /// @see GraphQL Spec: "StringValue"
    struct StringValue : sor< BlockString, seq< not_at< TripleQuote >, QuotedString > > {};

    /// @see GraphQL Spec: "NameStart"
    struct NameStart : sor<Letter,one<'_'>>{};

    /// @see GraphQL Spec: "NameContinue"
    struct NameContinue : sor<Letter,Digit,one<'_'>>{};

    /// @see GraphQL Spec: "Name"
    struct Name: seq<  NameStart, star<NameContinue>  >{};

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

    /// A single separator unit: whitespace, comment, or insignificant comma.
    struct TSeparator : sor<WhitespaceOnly, Comment, Comma> { };

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
            : seq<TSeparator, TRule0, SeqWithComments<TSeparator, TRulesRest...> > {
    };

    template<typename TSeparator, typename TRule0>
    struct SeqWithComments<TSeparator, TRule0>
            : seq<TSeparator, TRule0 > {
    };
    /** @} */


    /** @defgroup gql_types Type grammar building blocks (demo)
     *  @brief Basic type system constructs for tests and examples.
     *  @{ */

    /// NamedType (identifier) in our demo grammar.
    struct TypeName : SeqWithComments<
                TSeps,
                identifier,
                TSeps
    >{};

    /// Built-in scalar: String
    struct String : SeqWithComments<
                TSeps,
                string<'S','t','r','i','n','g'>,
                TSeps
    >{};

    /// Built-in scalar: Int
    struct Int : SeqWithComments<
                TSeps,
                string<'I','n','t'>,
                TSeps
    >{};

    /// Built-in scalar: Float
    struct Float : SeqWithComments<
                TSeps,
                string<'F','l','o','a','t'>,
                TSeps
    >{};

    /// Built-in scalar: Boolean
    struct Boolean : SeqWithComments<
                TSeps,
                string<'B','o','o','l','e','a','n'>,
                TSeps
    >{};

    /// Built-in scalar: ID
    struct ID : SeqWithComments<
                TSeps,
                string<'I','D'>,
                TSeps
    >{};

    /// Optional non-null marker '!'
    struct GqlNonNullType : opt<one<'!'>>{};

    struct BuiltInType;
    struct Type;

    /// Reference to a named type (identifier).
    struct NamedType : Name {};

    /// Any built-in scalar or nested array
    struct BuiltInType:sor<
                    String,
                    Int,
                    Float,
                    Boolean,
                    ID,
                    ListType
                >{};

    struct ScalarTypeDefinition : seq<
        TSeps,
        opt<Description>,
        TSeps,
        string<'s','c','a','l','a','r'>,
        TSeps,
        Name,
        TSeps,
        opt<DirectivesConst>
    >{};

    struct ImplementsInterfaces;

    struct InterfaceStart: seq<
        TSeps,
        ImplementsInterfaces,
        TSeps,
        string<'&'>
    >{};

    struct ImplementsInterfaces :
        SeqWithComments<
            TSeps,
            NamedType
        >
    {};

    struct Type: sor<NonNullType, ListType, NamedType>{};

    struct ListType : sor<
        seq< one<'['>, NonNullType, one<']'>>,
        seq< one<'['>, Type, one<']'> >
    >{};

    struct NonNullType : sor<
        seq<ListType, one<'!'>>,
        seq<NamedType, one<'!'>>
    >{};

    struct Variable : seq<one<'$'>, Name> {};

    struct VariableDefinition : SeqWithComments<
        TSeps,
        opt<Description>,
        Variable,
        one<':'>,
        Type,
        opt<DefaultValue>,
        opt<DirectivesConst>
    >
    {};

    struct VariableDefinitions : SeqWithComments<
        TSeps,
        one<'('>,
        TSeps,
        star<VariableDefinition>,
        one<')'>
    >
    {};

    struct BooleanValue : sor<string<'t','r','u','e'>, string<'f','a','l','s','e'>> {};

    struct NullValue : string<'n','u','l','l'> {};

    /// not true or false or null
    struct EnumValue : Name {};

    struct ListValue: sor<
        SeqWithComments<TSeps,one<'['>,star<seq<Value,TSeps>>,one<']'>>
            >{};

    struct ObjectField :
            SeqWithComments<TSeps,Name,TSeps,one<':'>,TSeps,Value>{};

    struct ObjectValue : SeqWithComments<
        TSeps,
        Beg,
        TSeps,
        star<ObjectField>,
        End
    >
    {};

    struct Value: sor
    <
        Variable,
        IntValue,
        FloatValue,
        StringValue,
        BooleanValue,
        NullValue,
        EnumValue,
        ListValue,
        ObjectValue
    >{};

    struct DefaultValue: Value {};

    struct InputValueDefinition : SeqWithComments<
        TSeps,
        opt<Description>,
        Name,
        one<':'>,
        Type,
        opt<DefaultValue>,
        opt<DirectivesConst>
    >{};

    struct ArgumentsDefinition : SeqWithComments<
        TSeps,
        one<'('>,
        plus<seq<InputValueDefinition,TSeps>>,
        one<')'>
    >{};

    struct Argument : SeqWithComments<
        TSeps,
        Name,
        one<':'>,
        Value
        >{};

    struct Arguments : SeqWithComments<
        TSeps,
        one<'('>,
        star<Argument>,
        one<')'>
    >{};

    /** @defgroup gql_exec Executable definitions
     *  @brief Minimal executable constructs used in tests (SelectionSet proxy, OperationType).
     *  @{ */

    struct Field;
    struct SelectionSet;

    /// Alias := Name ':'
    struct Alias : seq< Name, one<':'>, TSeps > {};

    /// Selection := Field | FragmentSpread (failure) | InlineFragment (failure)
    struct Selection : sor< Field > {};

    /** SelectionSet := '{' Selection* '}'
     *  Commas are Ignored tokens, so we use TSeps between selections.
     *  Using star instead of plus to support empty selection sets used in some tests.
     */
    struct SelectionSet : seq<
        Beg,
        TSeps,
        star< seq< Selection, TSeps > >,
        End
    > {};

    /// Field := Alias? Name Arguments? Directives? SelectionSet?
    struct Field : SeqWithComments<
        TSeps,
        opt<Alias>,
        Name,
        opt<Arguments>,
        opt<DirectivesConst>,
        opt<SelectionSet>
    > {};

    /** GqlSubQuery ~ minimal SelectionSet used in tests:
     *  Now pointing to more robust SelectionSet. */
    struct GqlSubQuery : SelectionSet {};

    /// Optional "query" keyword followed by a @ref GqlSubQuery selection set.
    struct GqlQuery : SeqWithComments<
                TSeps,
                opt<string<'q', 'u', 'e', 'r', 'y'> >,
                GqlSubQuery
            > {
    };
    /** @} */

    struct FieldDefinition : SeqWithComments<
        TSeps,
        opt<Description>,
        Name,
        opt<ArgumentsDefinition>,
        one<':'>,
        Type,
        opt<DirectivesConst>
    >{};

    struct FieldsDefinition : SeqWithComments<
        TSeps,
        Beg,
        star<FieldDefinition>,
        End
    >{};

    struct ObjectTypeDefinition : SeqWithComments<
        TSeps,
        opt<Description>,
        string<'t', 'y', 'p', 'e'>,
        Name,
        opt<ImplementsInterfaces>,
        opt<DirectivesConst>,
        FieldsDefinition
    > {};

    /// TypeDefinition (demo): 'type' Name '{' Field+ '}' with separators.
    struct TypeDefinition : sor<
        ScalarTypeDefinition,
        ObjectTypeDefinition
    >{};
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
    struct DirectiveDefinition; // placeholder
    struct SchemaDefinition;
    struct RootOperationTypeDefinition;
    struct OperationType;
    struct VariablesDefinition; // placeholder for now
    struct Directives; // placeholder for now
    struct SelectionSet; // alias to our subquery structure

    /// OperationType: 'query' | 'mutation' | 'subscription'
    struct OperationType : sor<
            string<'q','u','e','r','y'>,
            string<'m','u','t','a','t','i','o','n'>,
            string<'s','u','b','s','c','r','i','p','t','i','o','n'>
    >{};

    /// SelectionSet proxy used in tests (maps to @ref GqlSubQuery).
    // SelectionSet is already defined above at line 494
    // struct SelectionSet : GqlSubQuery {};

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
    // @see GraphQL Spec: "Description"
    struct Description : StringValue {};
    // Placeholder disabled to prevent empty matches that could cause non-consuming loops in Document
    struct FragmentDefinition : failure {};

    /** @defgroup gql_schema GraphQL schema/type-system and operation definitions
     *  @brief SchemaDefinition and related rules (plus OperationDefinition used in tests).
     *  @{ */
    /// @see GraphQL Spec: "OperationDefinition"
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

    /// @see GraphQL Spec: "ExecutableDefinition"
    struct ExecutableDefinition : sor< OperationDefinition, FragmentDefinition > {};

    /// @see GraphQL Spec: "TypeSystemDefinitionOrExtension"
    struct TypeSystemDefinitionOrExtension : sor<TypeSystemDefinition, TypeSystemExtension > {};

    struct TypeSystemDocument: plus< TypeSystemDefinition > {};

    struct DirectiveLocation : sor<
        string<'Q','U','E','R','Y'>,
        string<'M','U','T','A','T','I','O','N'>,
        string<'S','U','B','S','C','R','I','P','T','I','O','N'>,
        string<'F','R','A','G','M','E','N','T','_','D','E','F','I','N','I','T','I','O','N'>,
        string<'F','R','A','G','M','E','N','T','_','S','P','R','E','A','D'>,
        string<'I','N','L','I','N','E','_','F','R','A','G','M','E','N','T'>,
        string<'V','A','R','I','A','B','L','E','_','D','E','F','I','N','I','T','I','O','N'>,
        string<'S','C','H','E','M','A'>,
        string<'S','C','A','L','A','R'>,
        string<'O','B','J','E','C','T'>,
        string<'F','I','E','L','D','_','D','E','F','I','N','I','T','I','O','N'>,
        string<'F','I','E','L','D'>,
        string<'A','R','G','U','M','E','N','T','_','D','E','F','I','N','I','T','I','O','N'>,
        string<'I','N','T','E','R','F','A','C','E'>,
        string<'U','N','I','O','N'>,
        string<'E','N','U','M','_','V','A','L','U','E'>,
        string<'E','N','U','M'>,
        string<'I','N','P','U','T','_','F','I','E','L','D','_','D','E','F','I','N','I','T','I','O','N'>,
        string<'I','N','P','U','T','_','O','B','J','E','C','T'>
    > {};

    struct DirectiveLocations : seq<
        opt<seq<TSeps, one<'|'>>>,
        DirectiveLocation,
        star<seq<TSeps, opt<one<'|'>>, TSeps, DirectiveLocation>>
    > {};

    /// @see GraphQL Spec: "DirectiveDefinition"
    struct DirectiveDefinition : SeqWithComments<
            TSeps,
            opt<Description>,
            string<'d','i','r','e','c','t','i','v','e'>,
            TSeps,
            one<'@'>,
            Name,
            opt<ArgumentsDefinition>,
            opt<seq<TSeps, string<'o','n'>, TSeps, DirectiveLocations>>,
            TSeps
    > {};
    struct TypeSystemExtension : failure {};

    /// @see GraphQL Spec: "SchemaDefinition"
    struct SchemaDefinition : SeqWithComments<
            TSeps,
            opt<Description>,
            string<'s','c','h','e','m','a'>,
            opt<DirectivesConst>,
            Beg,
            plus< seq< RootOperationTypeDefinition, TSeps > >,
            End,
            TSeps
    > {};

    /// @see GraphQL Spec: "RootOperationTypeDefinition"
    struct RootOperationTypeDefinition : SeqWithComments<
            TSeps,
            OperationType,
            one<':'>,
            TypeName
    > {};

    /// @see GraphQL Spec: "TypeSystemDefinition"
    struct TypeSystemDefinition : sor<
        SchemaDefinition,
        TypeDefinition,
        DirectiveDefinition
        > {};

    /// @see GraphQL Spec: "Definition"
    struct Definition : sor< TypeSystemDefinitionOrExtension, ExecutableDefinition > {};

    /// @see GraphQL Spec: "ExecutableDocument"
    struct ExecutableDocument : plus< ExecutableDefinition > {};

    /** @defgroup gql_document Top-level Document rule and helpers
     *  @brief `Document` requires full input consumption and permits Ignored between/around definitions.
     *  @{ */
    /// Helper: zero or more Ignored tokens
    struct IgnoredMany : star< Ignored > {};

    /// @see GraphQL Spec: "Document"
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
            GqlQuery, Name, TypeDefinition, FieldDefinition,TypeName,Type,NamedType,
            Field, SelectionSet, Alias, Selection, DirectiveDefinition,
            String,Int,Float,Boolean,ID,ListType,GqlNonNullType,
            // New grammar nodes for Document/Schema
            Document, ExecutableDefinition, OperationDefinition, OperationType,
            SchemaDefinition, RootOperationTypeDefinition,
            // Types
            ScalarTypeDefinition,FieldDefinition,Arguments, Argument, ArgumentsDefinition, InputValueDefinition,
            DirectiveLocation, DirectiveLocations,
            ListType, NonNullType,Type,
            // Description
            Description,
            // Directives/Values
            DirectiveConst, DirectivesConst, ArgumentsConst, ArgumentConst, ValueConst,
            BooleanValue, NullValue, EnumValue, ListValue, ObjectValue, ObjectField,
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
     * @param p_in PEGTL string_input (will be restarted to print trace on failure or when requested).
     * @param p_query_name Label used in logs.
     * @param p_trace_on_success If true, run a PEGTL trace after successful parse as well.
     * @return (ok, root) where ok indicates parse success, and root is the parse tree (non-null on success).
     */
    template<typename TGrammar>
    std::tuple<bool,std::unique_ptr<node>>
    generate_ast_and_log(string_input<>& p_in, const std::string &p_query_name, const bool p_trace_on_success=false, bool p_print_dot=false) {
        std::unique_ptr<node> myRoot = pegtl::parse_tree::parse<
            TGrammar, GqlSelector /*, ns_pegtl::nothing, control*/
        >(p_in);
        bool myParsingOk;
        if (myRoot) {
            if (p_print_dot) {
                spdlog::debug("AST of \"{}\":\n{}", p_query_name, K_OUTPUT_SEP);
                pegtl::parse_tree::print_dot(std::cout, *myRoot);
                std::cout << endl
                        << K_OUTPUT_SEP << endl
                        << endl;
                std::cout.flush();
            }
            myParsingOk = true;
            if (p_trace_on_success) {
                p_in.restart();
                tao::pegtl::complete_trace<TGrammar>(p_in);
            }
        } else {
            spdlog::error(R"("{}" error / no AST generated!? Input rest:"{}".)", p_query_name,
                          std::string(p_in.begin(), p_in.size()));
            p_in.restart();
            tao::pegtl::complete_trace<TGrammar>(p_in);
            myParsingOk = false;
        }
        return {myParsingOk,std::move(myRoot)};
    }

    using TAstNodePtr = std::vector<std::unique_ptr<node>>::value_type;
    using TExpectedStr = std::expected<std::string, TErrorVector>;

    TExpectedStr ast_node_to_str(const TAstNodePtr &p_node);
    std::string dump_ast(const TAstNodePtr& ast);

    TAstNodePtr merge_type_definitions(TAstNodePtr &&p_schema_node, TAstNodePtr &&p_type_defs_node);

}
#endif //ISCHED_ISCHED_GQL_GRAMMAR_HPP